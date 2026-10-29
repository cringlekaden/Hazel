#pragma once
// Small maintained static composition. Public Scene APIs, no raw registry
// access.
static Hazel::Ref<Hazel::Scene> DropdownFixture()
{
    using namespace Hazel;
    using namespace GameUI;
    auto scene = CreateRef<Scene>();
    scene->OnViewportResize(400, 300);
    auto owner = scene->CreateEntityWithUUID(900, "Dropdown owner");
    auto canvas = scene->CreateEntityWithUUID(100, "Dropdown screen");
    auto &c = canvas.AddComponent<UICanvasComponent>();
    c.ReferenceSize = {400, 300};
    c.SafeInsets = {};
    c.Modal = true;
    c.Screen.Managed = true;
    c.Screen.EventOwner = 900;
    c.Screen.InitialFocus = 210;
    auto rect = [&](uint64_t id, const char *name, Entity p, glm::vec2 pos, glm::vec2 size)
    {
        auto e = scene->CreateEntityWithUUID(id, name);
        scene->Reparent(e, p, TransformPolicy::KeepLocal);
        auto &r = e.AddComponent<UIRectComponent>();
        r.Offset = pos;
        r.FixedSize = size;
        return e;
    };
    auto view = rect(150, "Invoking scroll", canvas, {210, 210}, {180, 70});
    view.AddComponent<UIScrollViewComponent>().Content = 151;
    view.AddComponent<SpriteRendererComponent>(glm::vec4(.05f, .08f, .12f, 1));
    auto content = rect(151, "Invoking content", view, {}, {180, 300});
    content.GetComponent<UIRectComponent>().WidthMode = SizeMode::Fill;
    auto group = rect(200, "Dropdown", content, {10, 40}, {160, 36});
    auto &g = group.AddComponent<UIRadioGroupComponent>();
    g.InitialSelection = 311;
    g.Action = "quality";
    auto &d = group.AddComponent<UIDropdownComponent>();
    d.Trigger = 210;
    d.Popup = 220;
    d.MaximumHeight = 120;
    auto trigger = rect(210, "Dropdown trigger", group, {}, {160, 36});
    auto &r = trigger.GetComponent<UIRectComponent>();
    r.WidthMode = r.HeightMode = SizeMode::Fill;
    r.Padding = {6, 4, 6, 4};
    trigger.AddComponent<UIButtonComponent>();
    trigger.AddComponent<SpriteRendererComponent>(glm::vec4(.15f, .25f, .35f, 1));
    trigger.AddComponent<TextComponent>().TextString = "Authored value stays intact";
    auto &ts = trigger.AddComponent<UITextStyleComponent>();
    ts.Size = 18;
    ts.Wrap = false;
    ts.MaxLines = 1;
    ts.Overflow = Text::Overflow::Ellipsis;
    auto popup = rect(220, "Dropdown popup", group, {}, {160, 120});
    popup.GetComponent<UIRectComponent>().Padding = {4, 4, 16, 4};
    popup.AddComponent<SpriteRendererComponent>(glm::vec4(.1f, .16f, .23f, 1));
    auto &sv = popup.AddComponent<UIScrollViewComponent>();
    sv.Content = 221;
    sv.VerticalTrack = 222;
    sv.VerticalThumb = 223;
    sv.RevealMargin = 0;
    auto list = rect(221, "Dropdown options", popup, {}, {140, 384});
    list.GetComponent<UIRectComponent>().WidthMode = SizeMode::Fill;
    list.GetComponent<UIRectComponent>().HeightMode = SizeMode::Content;
    list.AddComponent<UILayoutComponent>().Gap = 0;
    for (int i = 0; i < 12; ++i)
    {
        auto e = rect(300 + i, "Dropdown option", list, {}, {140, 32});
        auto &r = e.GetComponent<UIRectComponent>();
        r.WidthMode = SizeMode::Fill;
        r.Padding = {28, 4, 4, 4};
        auto &b = e.AddComponent<UIButtonComponent>();
        b.Kind = ControlKind::Radio;
        b.Group = 200;
        b.TabOrder = i;
        if (i == 0)
            e.GetComponent<TagComponent>().Tag = "Dropdown first option";
        if (i == 11)
            e.GetComponent<TagComponent>().Tag = "Dropdown last option";
        e.AddComponent<SpriteRendererComponent>(glm::vec4(.2f, .3f, .4f, 1));
        e.AddComponent<TextComponent>().TextString = "Choice " + std::to_string(i + 1);
        auto &style = e.AddComponent<UITextStyleComponent>();
        style.Size = 16;
        style.Wrap = false;
        style.Overflow = Text::Overflow::Ellipsis;
        style.MaxLines = 1;
    }
    auto track = rect(222, "Dropdown track", popup, {4, 0}, {8, 0});
    track.GetComponent<UIRectComponent>().Anchor.x = 1;
    track.GetComponent<UIRectComponent>().HeightMode = SizeMode::Fill;
    track.AddComponent<SpriteRendererComponent>(glm::vec4(.3f));
    rect(223, "Dropdown thumb", popup, {}, {8, 16})
        .AddComponent<SpriteRendererComponent>(glm::vec4(.8f));
    auto pressure = rect(400, "Pressure toggle", canvas, {10, 10}, {80, 40});
    pressure.AddComponent<SpriteRendererComponent>();
    auto &b = pressure.AddComponent<UIButtonComponent>();
    b.Kind = ControlKind::Toggle;
    b.Action = "pressure";
    b.TabOrder = 100;
    return scene;
}
