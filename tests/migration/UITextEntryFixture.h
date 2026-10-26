#pragma once
#include "UICompositionFixture.h"
#include "Hazel/Text/TextEditing.h"
struct SceneTextClipboard final : Hazel::Text::Clipboard
{
    std::string Value;
    bool Available = true;
    bool Read(std::string &value, size_t limit, std::string &error) override
    {
        if (!Available || Value.size() > limit)
        {
            error = "Clipboard unavailable/oversized";
            return false;
        }
        value = Value;
        return true;
    }
    bool Write(const std::string &value, std::string &) override
    {
        Value = value;
        return true;
    }
};
static Hazel::Ref<Hazel::Scene> TextEntryFixture()
{
    using namespace Hazel;using namespace Hazel::GameUI;
    auto s = CompositionFixture();
    auto parent = s->GetEntityByUUID(220);
    for (int i = 0; i < 2; ++i)
    {
        auto e = s->CreateEntityWithUUID(500 + i, i ? "Note field" : "Name field");
        s->Reparent(e, parent, TransformPolicy::KeepLocal);
        auto &r = e.AddComponent<UIRectComponent>();
        r.Offset = {8, i ? 110.f : 60.f};
        r.FixedSize = {260, i ? 80.f : 44.f};
        r.Padding = glm::vec4(8);
        auto &c = e.AddComponent<UITextEntryComponent>();
        c.Multiline = i != 0;
        c.MaxLines = i ? 8 : 1;
        c.Placeholder = i ? "Write a note" : "Name";
        auto &b = e.AddComponent<UIButtonComponent>();
        b.Kind = ControlKind::TextEntry;
        b.Action = i ? "note" : "name";
        b.TabOrder = 10 + i;
        e.AddComponent<TextComponent>().TextString =
            i ? "line one\nline two\nline three\nline four" : u8"a\u0301";
        auto &st = e.AddComponent<UITextStyleComponent>();
        st.Size = 20;
        st.Wrap = i != 0;
        st.Horizontal = st.Vertical = Align::Start;
        e.AddComponent<SpriteRendererComponent>(glm::vec4(.1f, .15f, .2f, 1));
    }
    return s;
}
