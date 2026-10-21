#include "hzpch.h"
#include "RuntimeUI.h"
#include "Hazel/Renderer/MSDFData.h"
#include <cmath>
namespace Hazel
{
using namespace GameUI;
glm::vec2 RuntimeUI::Intrinsic(Entity e, float width)
{
    glm::vec2 size{};
    if (e.HasComponent<TextComponent>())
    {
        auto &text = e.GetComponent<TextComponent>();
        auto style = e.HasComponent<UITextStyleComponent>() ? e.GetComponent<UITextStyleComponent>()
                                                            : UITextStyleComponent{};
        if (!std::isfinite(style.Size) || style.Size <= 0 || style.Size > 512 ||
            !std::isfinite(text.Kerning) || !std::isfinite(text.LineSpacing))
            throw std::invalid_argument("Invalid intrinsic UI text style/spacing");
        if (!text.TextString.empty() && text.TextString != "\xef\xbb\xbf" && width != 0)
        {
            auto &cache = m_Cache[e.GetUUID()];
            const auto assets = m_Scene.GetAssets();
            auto epoch = assets ? assets->Epoch() : 1;
            if (!cache.FontAsset || cache.Selection != text.Font || cache.Epoch != epoch)
            {
                cache.FontAsset = ResolveTextFont(text,assets);
                cache.Selection = text.Font;
                cache.Epoch = epoch;
                cache.Width = -1;cache.PreparedValid=false;
                cache.MeasureWidth = {{-2, -2}};
            }
            int slot = width < 0 ? 0 : 1;
            bool wrap = width >= 0 && style.Wrap;
            float normalizedWidth = width < 0 ? -1 : width / style.Size;
            if (cache.MeasureWidth[slot] != normalizedWidth ||
                cache.MeasureText[slot] != text.TextString ||
                cache.MeasureKerning[slot] != text.Kerning || cache.MeasureWrap[slot] != wrap)
            {
                Renderer2D::LayoutText(text.TextString, cache.FontAsset, normalizedWidth, wrap,
                                       text.Kerning, cache.MeasureLines[slot]);
                cache.MeasureWidth[slot] = normalizedWidth;
                cache.MeasureText[slot] = text.TextString;
                cache.MeasureKerning[slot] = text.Kerning;
                cache.MeasureWrap[slot] = wrap;
                ++m_Diagnostics.Measurements;
            }
            else
                ++m_Diagnostics.MeasurementReuses;
            auto &lines = cache.MeasureLines[slot];
            for (auto &line : lines)
                size.x = std::max(size.x, line.Width * style.Size);
            auto metrics = cache.FontAsset->GetMSDFData()->FontGeometry.getMetrics();
            float normalized = 1.f / float(metrics.ascenderY - metrics.descenderY);
            float lineHeight =
                (float(metrics.lineHeight) * normalized + text.LineSpacing) * style.Size;
            size.y = lines.empty() ? 0 : style.Size + (lines.size() - 1) * lineHeight;
            if (!std::isfinite(size.y) || size.y < 0)
                throw std::invalid_argument("Intrinsic text height is invalid");
        }
    }
    if (e.HasComponent<SpriteRendererComponent>())
    {
        auto &source = e.GetComponent<SpriteRendererComponent>().Source;
        auto &cache = m_Cache[e.GetUUID()];
        auto assets = m_Scene.GetAssets();
        auto epoch = assets ? assets->Epoch() : 1;
        glm::vec2 resourceExtent{-1, -1};
        bool same = source.index() == cache.MeasureImageSource.index();
        if (auto t = std::get_if<TextureSpriteSource>(&source))
        {
            if (t->Resource)
                resourceExtent = glm::vec2(t->Resource->GetWidth(), t->Resource->GetHeight());
            if (same)
            {
                const auto &old = std::get<TextureSpriteSource>(cache.MeasureImageSource);
                same = t->Texture == old.Texture && t->Resource == old.Resource;
            }
        }
        if (auto s = std::get_if<SpriteReference>(&source); s && same)
            same = *s == std::get<SpriteReference>(cache.MeasureImageSource);
        if (same && cache.MeasureImageEpoch == epoch &&
            cache.MeasureResourceExtent == resourceExtent && cache.MeasureImageSize.x >= 0)
        {
            ++m_Diagnostics.ImageMeasurementReuses;
            return glm::max(size, cache.MeasureImageSize);
        }
        glm::vec2 pixels{};
        if (auto t = std::get_if<TextureSpriteSource>(&source); t && t->Resource)
            pixels = glm::vec2(t->Resource->GetWidth(), t->Resource->GetHeight());
        else if (source.index())
        {
            bool unassigned = false;
            if (auto t = std::get_if<TextureSpriteSource>(&source))
                unassigned = t->Texture.empty();
            if (auto s = std::get_if<SpriteReference>(&source))
                unassigned = s->Sheet.empty() || !s->Region;
            if (!unassigned)
            {
                if (!assets)
                    throw std::invalid_argument("Intrinsic UI image needs project assets");
                auto sprite = assets->Resolve(source);
                if (sprite->Texture)
                    pixels = sprite->PixelSize();
            }
        }
        cache.MeasureImageSource = source;
        cache.MeasureImageEpoch = epoch;
        cache.MeasureResourceExtent = resourceExtent;
        cache.MeasureImageSize = pixels;
        ++m_Diagnostics.ImageMeasurements;
        size = glm::max(size, pixels);
    }
    return size;
}
void RuntimeUI::MeasureTree(Entity root, Rect parent)
{
    m_MeasureIDs.clear();
    std::function<void(Entity, size_t)> collect = [&](Entity e, size_t depth)
    {
        if (!e || !m_Scene.IsEntityValid(e.GetUUID()) || e.HasComponent<UICanvasComponent>() ||
            !e.HasComponent<UIRectComponent>())
            return;
        if (depth >= Scene::MaxDepth)
            throw std::invalid_argument("Measured UI exceeds depth 256");
        m_MeasureIDs.push_back(e.GetUUID());
        for (auto id : m_Scene.GetChildrenView(e.GetUUID()))
            collect(m_Scene.GetEntityByUUID(id), depth + 1);
    };
    collect(root, 0);
    if (m_MeasureNodes.size() < m_MeasureIDs.size())
        m_MeasureNodes.resize(m_MeasureIDs.size());
    for (size_t i = 0; i < m_MeasureIDs.size(); ++i)
        m_MeasureNodes[i].Children.clear();
    for (size_t i = 0; i < m_MeasureIDs.size(); ++i)
    {
        auto e = m_Scene.GetEntityByUUID(m_MeasureIDs[i]);
        auto &n = m_MeasureNodes[i];
        auto &r = e.GetComponent<UIRectComponent>();
        auto &out = m_Build.Elements[m_Build.Index.at(e.GetUUID())];
        n.ID = e.GetUUID();
        n.Name = e.GetName();
        n.Layout = r;
        n.Container = e.HasComponent<UILayoutComponent>();
        if (n.Container)
            n.Flow = e.GetComponent<UILayoutComponent>();
        n.Weight = r.Flex > 0 ? r.Flex : 1;
        n.Visible = out.Visible;
        out.MeasureIndex = i;
        n.Parent =
            i ? m_Build.Elements[m_Build.Index.at(m_Scene.GetRelationship(e).Parent)].MeasureIndex
              : std::numeric_limits<size_t>::max();
        if (i)
            m_MeasureNodes[n.Parent].Children.push_back(i);
    }
    try
    {
        m_Measure.Resolve(
            m_MeasureNodes, parent,
            [&](size_t index, float width)
            {
                auto e = m_Scene.GetEntityByUUID(m_MeasureIDs[index]);
                try
                {
                    return Intrinsic(e, width);
                }
                catch (const std::exception &error)
                {
                    throw std::invalid_argument(
                        "UI visual '" + e.GetName() + "'#" + std::to_string(m_MeasureIDs[index]) +
                        (width < 0 ? ".width: " : ".height: ") + error.what());
                }
            },
            m_MeasureIDs.size());
    }
    catch (const std::exception &error)
    {
        throw std::invalid_argument("Scene '" + m_Scene.GetName() + "' #" +
                                    std::to_string(m_Scene.GetIdentity()) + ": " + error.what());
    }
    for (size_t i = 0; i < m_MeasureIDs.size(); ++i)
    {
        auto &out = m_Build.Elements[m_Build.Index.at(m_MeasureIDs[i])];
        const auto &measured = m_Measure.Results()[i];
        out.Bounds = measured.Bounds;
        out.Available = measured.Available;
        out.Measured = measured.Measured;
        out.Overflow = measured.Overflow;
        out.Slot=measured.Slot;out.GridRow=measured.Row;out.GridColumn=measured.Column;out.Line=measured.Line;
        out.MeasureResolved = true;
        if (m_MeasureNodes[i].Visible)
            ++m_Diagnostics.Arrangements;
    }
    for(const auto &g:m_Measure.Guides())m_Build.Guides.push_back({m_MeasureIDs[g.Node],g.Axis,g.Index,g.Line,g.Bounds});
}
} // namespace Hazel
