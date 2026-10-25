#pragma once
static Hazel::Ref<Hazel::Scene> CompositionFixture()
{
    using namespace Hazel;
    using namespace GameUI;
    auto scene = CreateRef<Scene>();
    scene->OnViewportResize(400, 300);
    scene->CreateEntityWithUUID(900, "Composition owner");
    auto canvas = scene->CreateEntityWithUUID(100, "Composition screen");
    auto &c = canvas.AddComponent<UICanvasComponent>();
    c.ReferenceSize = {400, 300};
    c.SafeInsets = {};
    c.Modal = true;
    c.Screen.Managed = true;
    c.Screen.EventOwner = 900;
    c.Screen.InitialFocus = 210;
    auto rect = [&](uint64_t id, const char *name, Entity parent, glm::vec2 pos, glm::vec2 size) {
        auto e = scene->CreateEntityWithUUID(id, name);
        scene->Reparent(e, parent, TransformPolicy::KeepLocal);
        auto &r = e.AddComponent<UIRectComponent>();
        r.Offset = pos;
        r.FixedSize = size;
        return e;
    };
    auto button = [&](uint64_t id, const char *name, Entity parent, glm::vec2 pos, glm::vec2 size) {
        auto e = rect(id, name, parent, pos, size);
        e.AddComponent<SpriteRendererComponent>(glm::vec4(.15f, .2f, .25f, 1));
        e.AddComponent<TextComponent>().TextString = name;
        e.AddComponent<UITextStyleComponent>().Size = 16;
        e.AddComponent<UIButtonComponent>().Action = name;
        return e;
    };
    button(400, "Outside", canvas, {}, {400, 300});
    auto group = rect(200, "Tabs fixture", canvas, {20, 20}, {360, 260});
    group.AddComponent<UITabsComponent>();
    auto &g = group.AddComponent<UIRadioGroupComponent>();
    g.InitialSelection = 210;
    g.Action = "page";
    auto strip = rect(201, "Tab strip", group, {}, {360, 40});
    strip.AddComponent<UILayoutComponent>().Direction = Axis::Horizontal;
    for (int i = 0; i < 2; ++i)
    {
        auto tab = button(210 + i, i ? "Journal tab" : "Settings tab", strip, {}, {176, 40});
        tab.GetComponent<UIRectComponent>().Padding.x = 32;
        auto &b = tab.GetComponent<UIButtonComponent>();
        b.Kind = ControlKind::Radio;
        b.Group = 200;
        b.TabOrder = i;
        auto page = rect(220 + 10 * i, i ? "Journal page" : "Settings page", group, {}, {360, 210});
        auto &r = page.GetComponent<UIRectComponent>();
        r.WidthMode = r.HeightMode = SizeMode::Fill;
        r.Margin.y = 50;
        page.AddComponent<SpriteRendererComponent>(i ? glm::vec4(.1f, .15f, .3f, 1)
                                                     : glm::vec4(.3f, .15f, .1f, 1));
        auto &t = tab.AddComponent<UITabComponent>();
        t.Page = page.GetUUID();
        t.InitialFocus = i ? 231 : 221;
        if (!i)
            button(221, "Settings action", page, {10, 10}, {180, 40});
        else
        {
            auto view = rect(232, "Journal viewport", page, {8, 8}, {330, 150});
            view.AddComponent<UIScrollViewComponent>().Content = 233;
            auto content = rect(233, "Journal content", view, {}, {330, 400});
            auto option = button(231, "Journal action", content, {8, 250}, {200, 40});
            option.AddComponent<UITooltipComponent>().Body = 241;
            option.GetComponent<UITooltipComponent>().Delay = .1f;
            auto body = rect(241, "Purpose tooltip", option, {}, {200, 1});
            auto &br = body.GetComponent<UIRectComponent>();
            br.HeightMode = SizeMode::Content;
            br.Padding = glm::vec4(8);
            body.AddComponent<SpriteRendererComponent>(glm::vec4(.02f, .03f, .04f, 1));
            body.AddComponent<TextComponent>().TextString =
                "Opens this journal entry without losing your place.";
            auto &style = body.AddComponent<UITextStyleComponent>();
            style.Size = 16;
            style.MaxLines = 4;
            style.Overflow = Text::Overflow::Ellipsis;
        }
    }
    return scene;
}
