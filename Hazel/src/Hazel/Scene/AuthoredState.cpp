#include "hzpch.h"
#include "Hazel/Scripting/ScriptEngine.h"
#include "SceneSerializer.h"
#include <algorithm>
namespace Hazel
{
namespace
{
struct State
{
    std::string Bytes;
    template <class T> void Value(T value)
    {
        static_assert(std::is_arithmetic_v<T>);
        Bytes.append(reinterpret_cast<const char *>(&value), sizeof(value));
    }
    void Value(UUID value)
    {
        Value(uint64_t(value));
    }
    void Value(const std::string &value)
    {
        Value(uint64_t(value.size()));
        Bytes.append(value);
    }
    void Value(const std::filesystem::path &value)
    {
        Value(value.generic_u8string());
    }
    template <glm::length_t L, class T, glm::qualifier Q> void Value(const glm::vec<L, T, Q> &v)
    {
        for (int i = 0; i < L; i++)
            Value(v[i]);
    }
    void Source(const SpriteSource &source)
    {
        Value(uint32_t(source.index()));
        if (auto t = std::get_if<TextureSpriteSource>(&source))
        {
            Value(t->Texture);
            Value(t->TilingFactor);
        }
        if (auto r = std::get_if<SpriteReference>(&source))
        {
            Value(r->Sheet);
            Value(r->Region);
        }
    }
    template <class T, class F> void Component(Entity e, F write)
    {
        Value(e.HasComponent<T>());
        if (e.HasComponent<T>())
            write(e.GetComponent<T>());
    }
    void Field(const ScriptFieldInstance &f)
    {
        Value(uint32_t(f.Field.Type));
#define FIELD(kind, type)                                                                          \
    case ScriptFieldType::kind:                                                                    \
        Value(f.GetValue<type>());                                                                 \
        break
        switch (f.Field.Type)
        {
            FIELD(Float, float);
            FIELD(Double, double);
            FIELD(Bool, bool);
            FIELD(Char, uint16_t);
            FIELD(Byte, int8_t);
            FIELD(Short, int16_t);
            FIELD(Int, int32_t);
            FIELD(Long, int64_t);
            FIELD(UByte, uint8_t);
            FIELD(UShort, uint16_t);
            FIELD(UInt, uint32_t);
            FIELD(ULong, uint64_t);
            FIELD(Vector2, glm::vec2);
            FIELD(Vector3, glm::vec3);
            FIELD(Vector4, glm::vec4);
            FIELD(Entity, uint64_t);
        case ScriptFieldType::Prefab:
            Value(f.AssetReference);
            break;
        case ScriptFieldType::Sprite:
        case ScriptFieldType::SpriteAnimation:
            Value(f.AssetReference);
            Value(f.AssetID);
            break;
        case ScriptFieldType::None:
            break;
        }
#undef FIELD
    }
};
} // namespace
std::string SceneSerializer::CaptureAuthoredState() const
{
    State state;
    state.Bytes.reserve(m_Scene->GetAllEntitiesWith<IDComponent>().size() * 160);
    state.Value(m_Scene->GetName());
    std::vector<uint64_t> ids;
    for (auto h : m_Scene->GetAllEntitiesWith<IDComponent>())
        ids.push_back(Entity(h, m_Scene.get()).GetUUID());
    std::sort(ids.begin(), ids.end());
    state.Value(uint64_t(ids.size()));
    for (auto id : ids)
    {
        auto e = m_Scene->GetEntityByUUID(id);
        state.Value(id);
        auto r = m_Scene->GetRelationship(e);
        state.Value(r.Parent);
        state.Value(r.Order);
        state.Component<TagComponent>(e, [&](auto &c) { state.Value(c.Tag); });
        state.Component<TransformComponent>(e, [&](auto &c) {
            state.Value(c.Translation);
            state.Value(c.Rotation);
            state.Value(c.Scale);
        });
        state.Component<CameraComponent>(e, [&](auto &c) {
            auto &a = c.Camera;
            state.Value(int(a.GetProjectionType()));
            state.Value(a.GetPerspectiveVerticalFOV());
            state.Value(a.GetPerspectiveNearClip());
            state.Value(a.GetPerspectiveFarClip());
            state.Value(a.GetOrthographicSize());
            state.Value(a.GetOrthographicNearClip());
            state.Value(a.GetOrthographicFarClip());
            state.Value(c.Primary);
            state.Value(c.FixedAspectRatio);
        });
        state.Component<ScriptComponent>(e, [&](auto &c) {
            state.Value(c.ClassName);
            const auto &fields = ScriptEngine::GetScriptFieldMap(e);
            std::vector<std::string> names;
            for (auto &[name, f] : fields)
                if (f.Field.Type != ScriptFieldType::None)
                    names.push_back(name);
            std::sort(names.begin(), names.end());
            state.Value(uint64_t(names.size()));
            for (auto &name : names)
            {
                state.Value(name);
                state.Field(fields.at(name));
            }
        });
        state.Component<SpriteRendererComponent>(e, [&](auto &c) {
            state.Value(c.Color);
            state.Source(c.Source);
        });
        state.Component<SpriteAnimationComponent>(e, [&](auto &c) {
            state.Value(c.DefaultClip.Sheet);
            state.Value(c.DefaultClip.Clip);
            state.Value(c.Autoplay);
            state.Value(c.Speed);
        });
        state.Component<AudioSourceComponent>(e, [&](auto &c) {
            state.Value(c.Clip);
            state.Value(c.Gain);
            state.Value(c.Loop);
            state.Value(c.PlayOnStart);
        });
        state.Component<CircleRendererComponent>(e, [&](auto &c) {
            state.Value(c.Color);
            state.Value(c.Thickness);
            state.Value(c.Fade);
        });
        state.Component<Rigidbody2DComponent>(e, [&](auto &c) {
            state.Value(int(c.Type));
            state.Value(c.FixedRotation);
            state.Value(c.GravityScale);
        });
        state.Component<BoxCollider2DComponent>(e, [&](auto &c) {
            state.Value(c.Offset);
            state.Value(c.Size);
            state.Value(c.Density);
            state.Value(c.Friction);
            state.Value(c.Restitution);
            state.Value(c.RestitutionThreshold);
        });
        state.Component<CircleCollider2DComponent>(e, [&](auto &c) {
            state.Value(c.Offset);
            state.Value(c.Radius);
            state.Value(c.Density);
            state.Value(c.Friction);
            state.Value(c.Restitution);
            state.Value(c.RestitutionThreshold);
        });
        state.Component<UICanvasComponent>(e, [&](auto &c) {
            state.Value(c.ReferenceSize);
            state.Value(c.SafeInsets);
            state.Value(c.SortOrder);
            state.Value(c.Visible);
            state.Value(c.Enabled);
            state.Value(c.Modal);
            state.Value(c.Screen.Managed);state.Value(c.Screen.InitiallyOpen);state.Value(unsigned(c.Screen.Cancel));
            state.Value(c.Screen.InitialFocus);state.Value(c.Screen.DefaultAction);state.Value(c.Screen.CancelTarget);state.Value(c.Screen.ReturnFocus);state.Value(c.Screen.EventOwner);
        });
        state.Component<UIRectComponent>(e, [&](auto &c) {
            {
                state.Value(int(c.WidthMode));state.Value(int(c.HeightMode));
                state.Value(c.FixedSize);state.Value(c.MaximumSize);
                state.Value(bool(c.HasMaximum.x));state.Value(bool(c.HasMaximum.y));
            }
            state.Value(c.Anchor);
            state.Value(c.Offset);
            state.Value(c.Pivot);
            state.Value(c.MinimumSize);
            state.Value(c.Margin);
            state.Value(c.Padding);
            state.Value(c.Visible);
            state.Value(c.Enabled);
            state.Value(c.ClipChildren);
            state.Value(c.Flex);state.Value(c.GridRow);state.Value(c.GridColumn);state.Value(c.RowSpan);state.Value(c.ColumnSpan);
            state.Value(int(c.Fit));state.Value(c.ImageAlignment);
        });
        state.Component<UILayoutComponent>(e, [&](auto &c) {
            state.Value(int(c.Direction));
            state.Value(int(c.Main));
            state.Value(int(c.Cross));
            state.Value(c.Gap);state.Value(int(c.Kind));state.Value(c.LineGap);state.Value(int(c.LineAlign));state.Value(c.GridGap);
            for(const auto *tracks:{&c.Columns,&c.Rows}){state.Value(uint64_t(tracks->size()));for(const auto&t:*tracks){state.Value(int(t.Mode));state.Value(t.Value);state.Value(t.Minimum);state.Value(t.Maximum);state.Value(t.HasMaximum);}}
        });
        state.Component<UITextStyleComponent>(e, [&](auto &c) {
            state.Value(c.Size);
            state.Value(int(c.Horizontal));
            state.Value(int(c.Vertical));
            state.Value(c.Wrap);state.Value(int(c.Overflow));state.Value(c.MaxLines);
        });
        state.Component<UINineSliceComponent>(e, [&](auto &c) {
            state.Value(c.Borders);
            state.Value(c.BorderScale);
        });
        state.Component<UIButtonComponent>(e, [&](auto &c) {
            state.Value(unsigned(c.Kind));state.Value(c.InitiallyChecked);state.Value(c.Indicator);state.Value(c.Group);state.Value(c.EventOwner);
            state.Value(c.Action);
            state.Value(c.TabOrder);
            state.Value(c.Interactable);
            for (auto color : c.Colors)
                state.Value(color);
            for (const auto &source : c.Images)
                state.Source(source);
        });
        state.Component<UIRadioGroupComponent>(e,[&](auto &c){state.Value(c.Required);state.Value(c.InitialSelection);state.Value(c.EventOwner);state.Value(c.Action);});
        state.Component<UISliderComponent>(e,[&](auto &c){state.Value(c.Minimum);state.Value(c.Maximum);state.Value(c.Step);state.Value(c.InitialValue);state.Value(c.Track);state.Value(c.Fill);state.Value(c.Thumb);state.Value(unsigned(c.Direction));});
        state.Component<UIProgressComponent>(e,[&](auto &c){state.Value(c.Minimum);state.Value(c.Maximum);state.Value(c.InitialValue);state.Value(c.Fill);state.Value(unsigned(c.Direction));});
        state.Component<TextComponent>(e, [&](auto &c) {
            state.Value(c.TextString);
            state.Value(c.Font.Family);state.Value(int(c.Font.Style));
            state.Value(c.Color);
            state.Value(c.Kerning);
            state.Value(c.LineSpacing);
        });
    }
    return std::move(state.Bytes);
}
} // namespace Hazel
