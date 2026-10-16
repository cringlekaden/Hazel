#include "UILayoutConversion.h"
#include <cmath>
#include <stdexcept>
namespace Hazel
{
namespace
{
bool SameRect(const UIRectComponent &a, const UIRectComponent &b)
{
    UIEditValues first, second;
    first.Rect = a;
    second.Rect = b;
    return first.Same(second);
}
class ConversionCommand final : public DocumentCommand
{
  public:
    ConversionCommand(uint64_t document, std::vector<UILayoutConversion::Entry> entries)
        : m_Document(document), m_Entries(std::move(entries))
    {
    }
    const std::string &Name() const override
    {
        static const std::string name = "Convert UI layout";
        return name;
    }
    size_t Bytes() const override
    {
        size_t bytes = sizeof(*this) + m_Entries.capacity() * sizeof(UILayoutConversion::Entry);
        for (auto &e : m_Entries)
            bytes += e.Guard->Bytes();
        return bytes;
    }
    bool Changed() const override { return !m_Entries.empty(); }
    void Check(Scene &scene) const override
    {
        if (scene.GetIdentity() != m_Document || scene.IsRunning() || scene.IsSimulating() ||
            scene.IsStopping())
            throw std::invalid_argument("Conversion belongs to another or running document");
        for (auto &entry : m_Entries)
        {
            entry.Guard->Check(scene);
            auto e = scene.GetEntityByUUID(entry.ID);
            if (!SameRect(e.GetComponent<UIRectComponent>(),
                          m_Published ? entry.After : entry.Before))
                throw std::invalid_argument("Converted UI Rect changed outside history; revert the "
                                            "conflicting edit or clear history");
        }
    }
    void Apply(Scene &scene, bool undo) override
    {
        Check(scene);
        if (undo != m_Published)
            throw std::invalid_argument("Conversion transaction state changed");
        for (auto &entry : m_Entries)
            scene.GetEntityByUUID(entry.ID).GetComponent<UIRectComponent>() =
                undo ? entry.Before : entry.After;
        try
        {
            scene.ValidateUILayoutDependencies();
            scene.RefreshUI();
        }
        catch (...)
        {
            for (auto &entry : m_Entries)
                scene.GetEntityByUUID(entry.ID).GetComponent<UIRectComponent>() =
                    undo ? entry.After : entry.Before;
            try
            {
                scene.RefreshUI();
            }
            catch (...)
            {
            }
            throw;
        }
        m_Published = !undo;
    }
    void Cancel(Scene &scene) override
    {
        if (m_Published)
            Apply(scene, true);
    }

  private:
    uint64_t m_Document;
    std::vector<UILayoutConversion::Entry> m_Entries;
    bool m_Published = false;
};
} // namespace
void UILayoutConversion::Preview(const Ref<Scene> &scene, uint64_t root, bool freezeAmbiguous)
{
    m_Entries.clear();
    m_Notes.clear();
    m_Error.clear();
    m_Valid = false;
    try
    {
        if (!scene || scene->IsRunning() || scene->IsSimulating())
            throw std::invalid_argument("Stop before previewing UI conversion");
        scene->RefreshUI();
        auto selected = scene->GetEntityByUUID(root);
        if (!selected || !scene->IsEntityValid(root))
            throw std::invalid_argument("Select a live UI subtree or Canvas");
        const auto &snapshot = scene->GetUISnapshot();
        m_Document = scene->GetIdentity();
        m_Generation = snapshot.Generation;
        bool blocked = false;
        size_t bytes = 0;
        for (auto id : scene->GetSubtree(selected))
        {
            auto e = scene->GetEntityByUUID(id);
            if (!e.HasComponent<UIRectComponent>() || e.HasComponent<UICanvasComponent>())
                continue;
            const auto &before = e.GetComponent<UIRectComponent>();
            if (before.LayoutVersion == 2)
                continue;
            auto resolved = snapshot.Find(id);
            if (!resolved || !resolved->LayoutResolved)
            {
                m_Notes.push_back(
                    e.GetName() +
                    ": no visible resolved geometry; reveal this subtree before conversion");
                blocked = true;
                continue;
            }
            auto parent = scene->GetEntityByUUID(scene->GetRelationship(e).Parent);
            bool flow = parent && !parent.HasComponent<UICanvasComponent>() &&
                        parent.HasComponent<UILayoutComponent>() &&
                        parent.HasComponent<UIRectComponent>();
            if (id == root && flow && parent.GetComponent<UIRectComponent>().LayoutVersion == 1)
            {
                m_Notes.push_back(
                    e.GetName() +
                    ": select the containing legacy Flow so its allocation converts atomically");
                blocked = true;
                continue;
            }
            Entry entry;
            entry.ID = id;
            entry.Before = before;
            entry.After = before;
            entry.After.LayoutVersion = 2;
            entry.Guard = CreateScope<UIEditCommand>(*scene, id, "Conversion lifetime guard",
                                                     UIEditValues::Capture(e));
            bytes += sizeof(Entry) + entry.Guard->Bytes();
            if (bytes > DocumentHistory::DefaultBytes)
                throw std::invalid_argument(
                    "Conversion exceeds the 64 MiB command budget; convert smaller subtrees");
            auto &after = entry.After;
            after.FixedSize = resolved->Bounds.Size;
            after.HasMaximum = {false, false};
            after.MaximumSize = {};
            after.WidthMode = after.HeightMode = GameUI::SizeMode::Fixed;
            for (int axis = 0; axis < 2; ++axis)
            {
                auto &mode = axis ? after.HeightMode : after.WidthMode;
                if (flow)
                {
                    const auto &layout = parent.GetComponent<UILayoutComponent>();
                    int main = layout.Direction == GameUI::Axis::Horizontal ? 0 : 1;
                    after.AnchorMin[axis] = after.AnchorMax[axis] = after.Offset[axis] =
                        after.Pivot[axis] = 0;
                    if (axis == main && before.Flex > 0)
                    {
                        mode = GameUI::SizeMode::Fill;
                        after.MinimumSize[axis] =
                            std::max(before.MinimumSize[axis], before.SizeDelta[axis]);
                    }
                    else if (axis != main && layout.Cross == GameUI::Align::Stretch)
                        mode = GameUI::SizeMode::Fill;
                    else
                        after.FixedSize[axis] =
                            std::max(before.SizeDelta[axis], before.MinimumSize[axis]);
                    continue;
                }
                float span = before.AnchorMax[axis] - before.AnchorMin[axis];
                if (span == 1)
                {
                    float leading = before.Offset[axis] -
                                    before.Pivot[axis] * before.SizeDelta[axis] +
                                    before.Margin[axis];
                    float trailing = -before.SizeDelta[axis] + before.Margin[axis] +
                                     before.Margin[axis + 2] - leading;
                    if (leading >= 0 && trailing >= 0)
                    {
                        mode = GameUI::SizeMode::Fill;
                        after.Margin[axis] = leading;
                        after.Margin[axis + 2] = trailing;
                        after.AnchorMin[axis] = after.AnchorMax[axis] = after.Offset[axis] =
                            after.Pivot[axis] = 0;
                        continue;
                    }
                }
                if (span != 0)
                {
                    m_Notes.push_back(
                        e.GetName() + (axis ? " height" : " width") +
                        ": stretch cannot map to full Fill with nonnegative insets; Fixed freezes "
                        "the preview size and loses responsive stretch");
                    if (!freezeAmbiguous)
                        blocked = true;
                }
                after.AnchorMax[axis] = after.AnchorMin[axis];
                after.Offset[axis] = resolved->Bounds.Position[axis] -
                                     resolved->ParentContent.Position[axis] -
                                     after.AnchorMin[axis] * resolved->ParentContent.Size[axis] +
                                     after.Pivot[axis] * after.FixedSize[axis] - after.Margin[axis];
            }
            GameUI::Validate(after);
            m_Entries.push_back(std::move(entry));
        }
        if (m_Entries.empty())
            throw std::invalid_argument("No convertible legacy UI Rects in this selection");
        if (blocked)
        {
            m_Error = "Resolve the listed cases, or explicitly freeze ambiguous stretches, then "
                      "preview again";
            return;
        }
        auto staged = Scene::Copy(scene);
        staged->OnViewportResize(uint32_t(snapshot.Surface.Viewport.x),
                                 uint32_t(snapshot.Surface.Viewport.y));
        staged->SetUIFramebufferSize(snapshot.Surface.Framebuffer);
        for (auto &entry : m_Entries)
            staged->GetEntityByUUID(entry.ID).GetComponent<UIRectComponent>() = entry.After;
        staged->ValidateUILayoutDependencies();
        staged->RefreshUI();
        for (auto &entry : m_Entries)
        {
            const auto *a = snapshot.Find(entry.ID), *b = staged->GetUISnapshot().Find(entry.ID);
            if (!b || !b->LayoutResolved ||
                glm::length(a->Bounds.Position - b->Bounds.Position) > .05f ||
                glm::length(a->Bounds.Size - b->Bounds.Size) > .05f)
                throw std::invalid_argument(
                    "Conversion does not preserve resolved appearance for entity #" +
                    std::to_string(entry.ID));
            if(a->FramebufferClip.Position!=b->FramebufferClip.Position || a->FramebufferClip.Size!=b->FramebufferClip.Size)
                throw std::invalid_argument("Conversion changes framebuffer coverage for entity #"+std::to_string(entry.ID)+"; retain legacy layout or author explicit Fixed sizing");
            m_Notes.push_back(
                scene->GetEntityByUUID(entry.ID).GetName() + ": " +
                std::to_string(a->Bounds.Size.x) + " x " + std::to_string(a->Bounds.Size.y) + "; " +
                (entry.After.WidthMode == GameUI::SizeMode::Fill ? "Fill" : "Fixed") + " / " +
                (entry.After.HeightMode == GameUI::SizeMode::Fill ? "Fill" : "Fixed"));
        }
        m_Valid = true;
    }
    catch (const std::exception &error)
    {
        m_Error = error.what();
    }
}
bool UILayoutConversion::Commit(Scene &scene, DocumentHistory &history)
{
    try
    {
        if (!m_Valid || scene.GetIdentity() != m_Document)
            throw std::invalid_argument("Preview conversion in the current document first");
        scene.RefreshUI();
        if (scene.GetUISnapshot().Generation != m_Generation)
            throw std::invalid_argument("Layout changed since preview; preview conversion again");
        auto command = CreateScope<ConversionCommand>(m_Document, std::move(m_Entries));
        if (!history.Begin(scene, std::move(command)))
            throw std::invalid_argument(history.Diagnostic());
        try
        {
            history.Transaction()->Apply(scene, false);
            if (!history.Commit(scene))
                throw std::invalid_argument(history.Diagnostic());
        }
        catch (...)
        {
            history.Cancel(scene);
            throw;
        }
        m_Valid = false;
        return true;
    }
    catch (const std::exception &error)
    {
        m_Error = error.what();
        m_Valid = false;
        return false;
    }
}
} // namespace Hazel
