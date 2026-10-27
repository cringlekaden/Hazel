#include "StructuralCommand.h"
#include "Hazel/Scene/SceneSerializer.h"
#include "Hazel/Assets/ProjectAssets.h"
#include <yaml-cpp/yaml.h>
#include <set>
namespace Hazel
{
namespace
{
using Nodes = std::map<uint64_t, YAML::Node>;
Ref<Scene> Borrow(Scene &scene)
{
    return Ref<Scene>(&scene, [](Scene *) {});
}
bool Generated(const SpriteSource &source)
{
    auto t = std::get_if<TextureSpriteSource>(&source);
    return t && t->Texture.empty() && t->Resource;
}
void Strip(SpriteSource &source)
{
    if (auto t = std::get_if<TextureSpriteSource>(&source))
        t->Resource.reset();
}
SpriteSource *Image(Entity e, int slot)
{
    if (!e)
        return nullptr;
    if (slot < 0)
        return e.HasComponent<SpriteRendererComponent>()
                   ? &e.GetComponent<SpriteRendererComponent>().Source
                   : nullptr;
    return e.HasComponent<UIButtonComponent>()
               ? &e.GetComponent<UIButtonComponent>().Images.at(size_t(slot))
               : nullptr;
}
std::optional<SpriteSource> ImageValue(Entity e, int slot)
{
    auto p = Image(e, slot);
    if (!p)
        return {};
    auto s = *p;
    Strip(s);
    return s;
}
bool Incomplete(const std::optional<SpriteSource> &source)
{
    if (!source)
        return false;
    if (auto t = std::get_if<TextureSpriteSource>(&*source))
        return t->Texture.empty();
    if (auto r = std::get_if<SpriteReference>(&*source))
        return !r->Region;
    return false;
}
YAML::Node Snapshot(Scene &scene)
{
    // Rare native procedural images are not document resources. Sanitize only a
    // disposable codec view; the live bridge is preserved by PatchAuthoring.
    bool generated = false;
    for (auto h : scene.GetAllEntitiesWith<SpriteRendererComponent>())
        generated |= Generated(Entity(h, &scene).GetComponent<SpriteRendererComponent>().Source);
    for (auto h : scene.GetAllEntitiesWith<UIButtonComponent>())
        for (auto &source : Entity(h, &scene).GetComponent<UIButtonComponent>().Images)
            generated |= Generated(source);
    auto view = generated ? Scene::CopyForAuthoring(Borrow(scene)) : Borrow(scene);
    if (generated)
    {
        for (auto h : view->GetAllEntitiesWith<SpriteRendererComponent>())
            Strip(Entity(h, view.get()).GetComponent<SpriteRendererComponent>().Source);
        for (auto h : view->GetAllEntitiesWith<UIButtonComponent>())
            for (auto &source : Entity(h, view.get()).GetComponent<UIButtonComponent>().Images)
                Strip(source);
    }
    return YAML::Load(SceneSerializer(view).SerializeAuthoredSnapshot({}, false));
}
Nodes Entities(const YAML::Node &doc)
{
    Nodes out;
    for (auto e : doc["Entities"])
        out.emplace(e["Entity"].as<uint64_t>(), e);
    return out;
}
std::string Encoded(const YAML::Node &node)
{
    return node.IsDefined() ? YAML::Dump(node) : std::string{};
}
void Opaque(Entity e)
{
    if (!e)
        return;
    auto generated = [](const SpriteSource &source) {
        auto t = std::get_if<TextureSpriteSource>(&source);
        return t && t->Texture.empty() && t->Resource;
    };
    bool image = e.HasComponent<SpriteRendererComponent>() &&
                 generated(e.GetComponent<SpriteRendererComponent>().Source);
    if (e.HasComponent<UIButtonComponent>())
        for (auto &s : e.GetComponent<UIButtonComponent>().Images)
            image |= generated(s);
    if (e.HasComponent<NativeScriptComponent>() || image)
        throw std::runtime_error(
            "This object contains native-only bindings/generated images. Assign serializable "
            "authored assets before structural removal/copy");
}
template <class... T>
void Components(ComponentGroup<T...>, Scene &scene, uint64_t id, HistoryStamp &stamp)
{
    (stamp.Components.emplace(typeid(T), scene.GetAuthoringComponentVersion(id, typeid(T))), ...);
}
template <class... T, class F> void EachComponent(ComponentGroup<T...>, F visit)
{
    (visit(typeid(T)), ...);
}
template <class... T, class F>
void PreserveUnowned(ComponentGroup<T...>, Scene &current, Scene &decoded,
                     const std::vector<uint64_t> &ids, F owned)
{
    SceneSerializer a(Borrow(current)), b(Borrow(decoded));
    for (auto id : ids)
    {
        auto from = current.GetEntityByUUID(id), to = decoded.GetEntityByUUID(id);
        if (!from || !to)
            continue;
        (
            [&] {
                if constexpr (!std::is_same_v<T, NativeScriptComponent>)
                {
                    if (owned(id, typeid(T)) || a.CaptureAuthoredState(from, &typeid(T)) ==
                                                    b.CaptureAuthoredState(to, &typeid(T)))
                        return;
                    // Only repair in-memory picker/metadata distinctions the file codec
                    // intentionally normalizes. Unowned components are never command data.
                    if (to.HasComponent<T>())
                        to.RemoveComponent<T>();
                    if (from.HasComponent<T>())
                        to.AddComponent<T>(from.GetComponent<T>());
                }
            }(),
            ...);
    }
}
std::string ExactComponent(Scene &scene, uint64_t id, std::type_index wanted)
{
    auto entity = scene.GetEntityByUUID(id);
    if (!entity)
        return {};
    SceneSerializer codec(Borrow(scene));
    std::string value;
    EachComponent(AllComponents{}, [&](const std::type_info &type) {
        if (wanted == type)
            value = codec.CaptureAuthoredState(entity, &type);
    });
    return value;
}
void Update(HistoryStamp &s, const HistoryRestore &r) noexcept
{
    if (s.Entity != r.Before.Entity || s.Lifetime != r.Before.Lifetime)
        return;
    s.Lifetime = r.After.Lifetime;
    if (s.Structure == r.Before.Structure)
        s.Structure = r.After.Structure;
    for (auto &[type, version] : s.Components)
    {
        auto a = r.Before.Components.find(type), b = r.After.Components.find(type);
        if (a != r.Before.Components.end() && b != r.After.Components.end() && version == a->second)
            version = b->second;
    }
}
} // namespace
HistoryStamp CaptureHistoryStamp(Scene &scene, uint64_t id)
{
    HistoryStamp s;
    s.Entity = id;
    s.Lifetime = scene.GetAuthoringLifetime(id);
    s.Structure = scene.GetAuthoringStructureVersion(id);
    Components(AllComponents{}, scene, id, s);
    return s;
}
StructuralCommand::StructuralCommand(const Ref<Scene> &before, const Ref<Scene> &after,
                                     std::string name, uint64_t a, uint64_t b)
    : m_Document(before->GetIdentity()), m_BeforeSelection(a), m_AfterSelection(b),
      m_Name(std::move(name))
{
    auto left = Entities(Snapshot(*before)), right = Entities(Snapshot(*after));
    std::set<uint64_t> ids;
    for (auto &[id, n] : left)
        ids.insert(id);
    for (auto &[id, n] : right)
        ids.insert(id);
    for (auto id : ids)
    {
        auto i = left.find(id), j = right.find(id);
        if (i == left.end() || j == right.end())
        {
            Opaque(before->GetEntityByUUID(id));
            Opaque(after->GetEntityByUUID(id));
            m_Records.push_back({id, "", i == left.end() ? "" : Encoded(i->second),
                                 j == right.end() ? "" : Encoded(j->second)});
        }
        else
        {
            std::set<std::string> keys;
            for (auto n : i->second)
                keys.insert(n.first.as<std::string>());
            for (auto n : j->second)
                keys.insert(n.first.as<std::string>());
            for (auto &key : keys)
            {
                auto a = Encoded(static_cast<const YAML::Node &>(i->second)[key]),
                     b = Encoded(static_cast<const YAML::Node &>(j->second)[key]);
                if (a != b)
                {
                    if (key == "SpriteRendererComponent" || key == "UIButtonComponent")
                        if (a.empty() || b.empty())
                            Opaque(before->GetEntityByUUID(id));
                    m_Records.push_back({id, key, std::move(a), std::move(b)});
                }
            }
        }
    }
    for (const auto &record : m_Records)
    {
        if (record.Key.empty() || record.Key == "SpriteRendererComponent" ||
            record.Key == "UIButtonComponent")
        {
            const int first = record.Key == "UIButtonComponent" ? 0 : -1,
                      last = record.Key == "SpriteRendererComponent" ? 0 : 5;
            for (int slot = first; slot < last; ++slot)
            {
                auto a = ImageValue(before->GetEntityByUUID(record.ID), slot),
                     b = ImageValue(after->GetEntityByUUID(record.ID), slot);
                if (Incomplete(a) || Incomplete(b))
                    m_PickerImages.push_back({record.ID, slot, std::move(a), std::move(b)});
            }
        }
    }
    for (const auto &r : m_Records)
        if (!m_Guards.count(r.ID))
            m_Guards.emplace(r.ID, Guards{CaptureHistoryStamp(*before, r.ID),
                                          CaptureHistoryStamp(*after, r.ID)});
    SceneSerializer beforeCodec(before), afterCodec(after);
    for (const auto &[id, guard] : m_Guards)
    {
        auto left = before->GetEntityByUUID(id), right = after->GetEntityByUUID(id);
        EachComponent(AllComponents{}, [&](const std::type_info &type) {
            auto x = left ? beforeCodec.CaptureAuthoredState(left, &type) : std::string{},
                 y = right ? afterCodec.CaptureAuthoredState(right, &type) : std::string{};
            if (!left || !right || x != y)
                m_Components.push_back({id, type, std::move(x), std::move(y)});
        });
    }
    std::set<uint64_t> boundary;
    for (const auto &source : {before, after})
        for (const auto &[id, guard] : m_Guards)
        {
            auto entity = source->GetEntityByUUID(id);
            for (size_t depth = 0; entity && depth < Scene::MaxDepth; ++depth)
            {
                auto parent = source->GetRelationship(entity).Parent;
                if (!uint64_t(parent))
                    break;
                if (!m_Guards.count(parent))
                    boundary.insert(parent);
                entity = source->GetEntityByUUID(parent);
            }
        }
    for (auto id : boundary)
        m_Boundaries.push_back(
            {id, before->GetAuthoringLifetime(id), after->GetAuthoringLifetime(id)});
    m_Restores.reserve(2 * m_Guards.size());
}
size_t StructuralCommand::Bytes() const
{
    size_t bytes = sizeof(*this) + m_Name.capacity() + m_Records.capacity() * sizeof(Record);
    for (auto &r : m_Records)
        bytes += r.Key.capacity() + r.Before.capacity() + r.After.capacity() + 3;
    // Conservatively include map nodes and up to two temporary restoration maps
    // per affected identity; no prepared scene/resource graph is an entry.
    bytes += m_Components.capacity() * sizeof(ComponentValue);
    for (const auto &c : m_Components)
        bytes += c.Before.capacity() + c.After.capacity() + 2;
    bytes += m_PickerImages.capacity() * sizeof(PickerImage);
    for (const auto &image : m_PickerImages)
        for (const auto *source : {&image.Before, &image.After})
            if (*source)
            {
                if (auto t = std::get_if<TextureSpriteSource>(&**source))
                    bytes += (t->Texture.native().capacity() + 1) *
                                 (sizeof(std::filesystem::path::value_type) +
                                  4 * sizeof(std::filesystem::path)) +
                             128;
                if (auto r = std::get_if<SpriteReference>(&**source))
                    bytes += (r->Sheet.native().capacity() + 1) *
                                 (sizeof(std::filesystem::path::value_type) +
                                  4 * sizeof(std::filesystem::path)) +
                             128;
            }
    bytes += m_Boundaries.capacity() * sizeof(Boundary);
    bytes += m_Guards.size() * (sizeof(Guards) + 96 + 6 * (sizeof(HistoryStamp) + 32 * 64));
    return bytes;
}
void StructuralCommand::CheckSide(Scene &scene, bool after) const
{
    if (scene.GetIdentity() != m_Document || scene.IsRunning() || scene.IsSimulating() ||
        scene.IsStopping())
        throw std::runtime_error("Structural history belongs to another/still-running document");
    for (auto &[id, g] : m_Guards)
    {
        const auto &expected = after ? g.After : g.Before;
        auto life = scene.GetAuthoringLifetime(id);
        if (life != expected.Lifetime)
            throw std::runtime_error("Structural target lifetime changed outside history; clear "
                                     "obsolete history or reopen the intended document");
        if (life && scene.GetAuthoringStructureVersion(id) != expected.Structure)
            throw std::runtime_error("Hierarchy/component structure changed outside history");
        if (life)
        {
            for (auto &[type, version] : expected.Components)
                if (scene.GetAuthoringComponentVersion(id, type) != version)
                    throw std::runtime_error(
                        "Component set/lifetime changed outside structural history");
        }
    }
    for (const auto &parent : m_Boundaries)
        if (scene.GetAuthoringLifetime(parent.ID) != (after ? parent.After : parent.Before))
            throw std::runtime_error("Hierarchy ancestor #" + std::to_string(parent.ID) +
                                     " was independently retired/replaced; restore the intended "
                                     "parent or clear obsolete history");
    auto nodes = Entities(Snapshot(scene));
    for (auto &r : m_Records)
    {
        auto it = nodes.find(r.ID);
        std::string value;
        if (it != nodes.end())
            value = r.Key.empty() ? Encoded(it->second)
                                  : Encoded(static_cast<const YAML::Node &>(it->second)[r.Key]);
        if (value != (after ? r.After : r.Before))
            throw std::runtime_error("Authored structural field changed outside history (entity " +
                                     std::to_string(r.ID) + ", " + r.Key +
                                     "); revert it or clear history");
    }
    for (const auto &component : m_Components)
        if (ExactComponent(scene, component.ID, component.Type) !=
            (after ? component.After : component.Before))
            throw std::runtime_error(
                "Owned authored component of entity #" + std::to_string(component.ID) +
                " changed outside history; revert it or clear obsolete history");
    for (const auto &image : m_PickerImages)
        if (!(ImageValue(scene.GetEntityByUUID(image.ID), image.Slot) ==
              (after ? image.After : image.Before)))
            throw std::runtime_error(
                "Unassigned image-picker value changed outside structural history");
}
void StructuralCommand::Check(Scene &scene) const
{
    CheckSide(scene, m_Applied);
}
void StructuralCommand::Apply(Scene &scene, bool undo)
{
    CheckSide(scene, undo); // Undo expects after; Redo expects before.
    auto document = Snapshot(scene);
    auto nodes = Entities(document);
    auto staged = Scene::CopyForAuthoring(Borrow(scene));
    for (auto &r : m_Records)
    {
        auto &value = undo ? r.Before : r.After;
        if (r.Key.empty())
        {
            if (value.empty())
                nodes.erase(r.ID);
            else
                nodes[r.ID] = YAML::Load(value);
        }
        else
        {
            auto it = nodes.find(r.ID);
            if (it == nodes.end())
                throw std::runtime_error("Structural entity missing");
            if (value.empty())
                it->second.remove(r.Key);
            else
                it->second[r.Key] = YAML::Load(value);
        }
    }
    document["Entities"] = YAML::Node(YAML::NodeType::Sequence);
    for (auto &[id, node] : nodes)
        document["Entities"].push_back(node);
    auto decoded = CreateRef<Scene>();
    decoded->SetAssets(scene.GetAssets());
    SceneSerializer codec(decoded,
                          scene.GetAssets() ? scene.GetAssets()->Root() : std::filesystem::path{},
                          true, scene.GetAssets());
    if (!codec.DeserializeText(YAML::Dump(document), false, ResourceLoading::MetadataOnly))
        throw std::runtime_error("Structural restore rejected: " + codec.Report().Error);
    std::vector<uint64_t> affected;
    affected.reserve(m_Guards.size());
    for (auto &[id, g] : m_Guards)
        affected.push_back(id);
    for (const auto &image : m_PickerImages)
    {
        const auto &value = undo ? image.Before : image.After;
        if (value)
        {
            auto to = Image(decoded->GetEntityByUUID(image.ID), image.Slot);
            if (!to)
                throw std::runtime_error("Restored image component is unavailable");
            *to = *value;
        }
    }
    PreserveUnowned(
        AllComponents{}, *staged, *decoded, affected, [&](uint64_t id, const std::type_info &type) {
            auto first =
                std::lower_bound(m_Components.begin(), m_Components.end(), id,
                                 [](const auto &c, uint64_t wanted) { return c.ID < wanted; });
            for (; first != m_Components.end() && first->ID == id; ++first)
                if (first->Type == type)
                    return true;
            return false;
        });
    staged->PatchAuthoring(decoded, affected);
    for (const auto &component : m_Components)
        if (ExactComponent(*staged, component.ID, component.Type) !=
            (undo ? component.Before : component.After))
            throw std::runtime_error(
                "Codec cannot restore this authored component exactly (entity #" +
                std::to_string(component.ID) +
                "); assign a complete portable reference before retrying");
    std::vector<HistoryRestore> mappings;
    mappings.reserve(2 * m_Guards.size());
    for (auto &[id, g] : m_Guards)
        if (staged->IsEntityValid(id))
        {
            auto next = CaptureHistoryStamp(*staged, id), current = CaptureHistoryStamp(scene, id);
            if (current.Lifetime)
                mappings.push_back({std::move(current), next});
            const auto &restored = undo ? g.Before : g.After;
            if (restored.Lifetime)
                mappings.push_back({restored, std::move(next)});
        }
    if (Bytes() > m_MemoryLimit)
        throw std::runtime_error("Structural history payload exceeds its admission limit");
    scene.PublishAuthoring(*staged);
    m_Restores.swap(mappings);
    m_Applied = !undo;
}
void StructuralCommand::Cancel(Scene &scene)
{
    if (m_Applied)
        Apply(scene, true);
}
void StructuralCommand::Reconcile(const std::vector<HistoryRestore> &mappings) noexcept
{
    for (auto &parent : m_Boundaries)
        for (const auto &change : mappings)
            if (parent.ID == change.Before.Entity)
            {
                if (parent.Before == change.Before.Lifetime)
                    parent.Before = change.After.Lifetime;
                if (parent.After == change.Before.Lifetime)
                    parent.After = change.After.Lifetime;
            }

    for (auto &[id, g] : m_Guards)
        for (const auto &r : mappings)
        {
            Update(g.Before, r);
            Update(g.After, r);
        }
}
} // namespace Hazel
