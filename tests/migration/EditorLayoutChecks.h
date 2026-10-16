#pragma once
#include "Authoring/UILayoutConversion.h"
namespace Hazel
{
static bool SameBounds(GameUI::Rect a, GameUI::Rect b)
{
    return glm::length(a.Position - b.Position) < .02f && glm::length(a.Size - b.Size) < .02f;
}
static void EditorLayoutChecks()
{
    using namespace GameUI;
    for (int recipe = 0; recipe < 3; ++recipe)
    {
        std::cout << "Layout conversion recipe " << recipe << "\n";
        HistoryFixture f;
        auto &r = f.Control.GetComponent<UIRectComponent>();
        if (recipe == 0)
        {
            r.Pivot = {.3f, .6f};
            r.Offset = {40, 50};
            r.SizeDelta = {-20, 30};
            r.MinimumSize = {80, 60};
        }
        if (recipe == 1)
        {
            r.AnchorMax = {1, 1};
            r.Pivot = {.2f, .4f};
            r.Offset = {12, 8};
            r.SizeDelta = {-80, -50};
            r.Margin = {3, 4, 5, 6};
            r.MinimumSize = {1400, 1000};
        }
        if (recipe == 2)
        {
            r.SizeDelta = {300, 200};
            r.Padding = glm::vec4(4);
            f.Control.AddComponent<UILayoutComponent>().Gap = 10;
            for (int i = 0; i < 2; ++i)
            {
                auto child = f.World->CreateEntity("Conversion flow child");
                f.World->Reparent(child, f.Control, TransformPolicy::KeepLocal);
                auto &c = child.AddComponent<UIRectComponent>();
                c.SizeDelta = {100, float(35 + 10 * i)};
                c.Flex = float(i + 1);
            }
        }
        f.World->RefreshUI();
        const auto before = f.State();
        std::vector<std::pair<uint64_t, Rect>> old;
        for (auto id : f.World->GetSubtree(f.Control))
            old.emplace_back(id, f.World->GetUISnapshot().Find(id)->Bounds);
        DocumentHistory h;
        h.Bind(f.World->GetIdentity());
        h.MarkSaved(before);
        {
            UILayoutConversion cancelled;
            cancelled.Preview(f.World, f.Control.GetUUID(), false);
            Check(cancelled.Valid() && f.State() == before && !h.CanUndo(),
                  "Conversion preview mutated source or rejected supported legacy layout");
        }
        UILayoutConversion conversion;
        conversion.Preview(f.World, f.Control.GetUUID(), false);
        Check(conversion.Valid(), conversion.Error());
        Check(conversion.Commit(*f.World, h) && h.Operations() == 1,
              "Subtree conversion did not commit as one shared edit");
        for (auto [id, bounds] : old)
            Check(SameBounds(bounds, f.World->GetUISnapshot().Find(id)->Bounds),
                  "Conversion changed current resolved appearance");
        const auto after = f.State();
        Check(after != before && h.Dirty(after), "Conversion missing authored/dirty fields");
        Check(h.Undo(*f.World) && f.State() == before && !h.Dirty(f.State()),
              "Conversion Undo did not restore exact legacy fields/save point");
        Check(h.Redo(*f.World) && f.State() == after,
              "Conversion Redo did not restore measured authored fields");
        f.World->OnViewportResize(960, 480);
        f.World->RefreshUI();
        auto responsive = f.World->GetUISnapshot().Find(f.Control.GetUUID())->Bounds;
        Check(h.Undo(*f.World), "Conversion Undo failed after viewport resize");
        Check(SameBounds(responsive, f.World->GetUISnapshot().Find(f.Control.GetUUID())->Bounds),
              "Conversion lost representable responsive intent");
        Check(h.Redo(*f.World), "Conversion Redo failed after viewport resize");
        auto serialized = SceneSerializer(f.World).SerializeText();
        auto loaded = CreateRef<Scene>();
        Check(SceneSerializer(loaded).DeserializeText(serialized) &&
                  SceneSerializer(loaded).CaptureAuthoredState() == after,
              "Converted save/reopen roundtrip lost sizing state");
        auto id = f.Control.GetUUID();
        auto kept = f.Control.GetComponent<UIRectComponent>();
        f.Control.RemoveComponent<UIRectComponent>();
        f.Control.AddComponent<UIRectComponent>(kept);
        auto changed = f.State();
        Check(!h.Undo(*f.World) && f.State() == changed,
              "Conversion Undo partially applied after component retirement");
        (void)id;
    }
    {
        HistoryFixture f;
        auto &r = f.Control.GetComponent<UIRectComponent>();
        r.AnchorMin.x = .25f;
        r.AnchorMax.x = .75f;
        f.World->RefreshUI();
        const auto before = f.State();
        DocumentHistory h;
        h.Bind(f.World->GetIdentity());
        UILayoutConversion preview;
        preview.Preview(f.World, f.Control.GetUUID(), false);
        Check(!preview.Valid() && !preview.Notes().empty() && f.State() == before,
              "Ambiguous stretch was silently converted");
        preview.Preview(f.World, f.Control.GetUUID(), true);
        Check(preview.Valid() && preview.Commit(*f.World, h) && h.Undo(*f.World) &&
                  f.State() == before,
              "Explicit freeze choice/cancel/Undo lost authored state");
        preview.Preview(f.World, f.Control.GetUUID(), true);
        r.Offset.y += 5;
        f.World->RefreshUI();
        Check(!preview.Commit(*f.World, h), "Stale conversion preview accepted changed layout");
        preview.Preview(f.World, f.Control.GetUUID(), true);
        HistoryFixture other;
        DocumentHistory ph;
        ph.Bind(other.World->GetIdentity());
        auto unchanged = other.State();
        Check(!preview.Commit(*other.World, ph) && other.State() == unchanged && !ph.CanUndo(),
              "Conversion applied to another document");
    }
    {
        HistoryFixture f;
        auto &r = f.Control.GetComponent<UIRectComponent>();
        r.LayoutVersion = 2;
        r.FixedSize = {160, 80};
        r.HeightMode = SizeMode::Content;
        auto &style = f.Control.AddComponent<UITextStyleComponent>();
        style.Size = 20;
        style.Wrap = true;
        f.World->RefreshUI();
        DocumentHistory h;
        h.Bind(f.World->GetIdentity());
        const auto initial = f.State();
        h.MarkSaved(initial);
        UIInspectorEditing edit;
        PropertyUI::EditActivity activity;
        activity.Item = 901;
        activity.Label = "Fixed width";
        activity.Changed = true;
        for (int i = 0; i < 12; ++i)
        {
            auto before = UIEditValues::Capture(f.Control), next = before;
            next.Rect->FixedSize.x += 1;
            Check(edit.Edit(*f.World, &h, f.Control.GetUUID(), "UI Rect", before, next, activity,
                            901),
                  edit.Error());
        }
        edit.Finish(*f.World, &h, false);
        Check(h.Operations() == 1 && r.FixedSize.x == 172,
              "Measured Inspector drag split history or wrote ignored SizeDelta");
        Check(h.Undo(*f.World) && f.State() == initial, "Measured Inspector Undo lost fields");
        auto constraintBefore = UIEditValues::Capture(f.Control), constraints = constraintBefore;
        constraints.Rect->WidthMode = SizeMode::Fill;
        constraints.Rect->MinimumSize.x = 20;
        constraints.Rect->HasMaximum.x = true;
        constraints.Rect->MaximumSize.x = 200;
        PropertyUI::EditActivity committed;
        committed.Changed = committed.Committed = true;
        Check(edit.Edit(*f.World, &h, f.Control.GetUUID(), "Sizing constraints", constraintBefore,
                        constraints, committed, 0), edit.Error());
        Check(r.WidthMode == SizeMode::Fill && r.HasMaximum.x && r.MaximumSize.x == 200 &&
                  f.World->GetUISnapshot().Find(f.Control.GetUUID())->Bounds.Size.x == 200 &&
                  h.Undo(*f.World) && f.State() == initial,
              "Inspector sizing mode/min/max failed to publish or Undo coherently");
        UIRectEditSession gesture;
        Check(!gesture.Begin(f.World, f.Control.GetUUID(), 8, &h) && !h.Active(),
              "Content height accepted ineffective viewport resizing");
        Check(gesture.Begin(f.World, f.Control.GetUUID(), 4, &h),
              "Measured Fixed resize failed to begin");
        for (int i = 1; i <= 12; ++i)
            Check(gesture.Update({float(i * 2), 0}), gesture.Error());
        gesture.Finish(false);
        Check(h.Operations() == 1 && r.FixedSize.x == 184 && !h.CanRedo(),
              "Measured complete gesture failed coalescing/redo branch invalidation");
        Check(h.Undo(*f.World) && f.State() == initial,
              "Measured gesture Undo did not restore authored state");
        Check(gesture.Begin(f.World, f.Control.GetUUID(), 0, &h) && gesture.Update({12, 8}),
              "Measured move setup failed");
        gesture.Finish(true);
        Check(f.State() == initial && !h.Active() && h.CanRedo(),
              "Measured Escape/cancel changed history or source");
        f.Control.AddComponent<UILayoutComponent>();
        auto child = f.World->CreateEntity("Fixed stack child");
        f.World->Reparent(child, f.Control, TransformPolicy::KeepLocal);
        auto &cr = child.AddComponent<UIRectComponent>();
        cr.LayoutVersion = 2;
        cr.FixedSize = {80, 30};
        cr.Offset = {9, 7};
        h.Clear();
        f.World->RefreshUI();
        Check(!gesture.Begin(f.World, child.GetUUID(), 0, &h) &&
                  !gesture.Begin(f.World, child.GetUUID(), 1, &h),
              "Stack-owned position accepted a move/leading-edge gesture");
        Check(gesture.Begin(f.World, child.GetUUID(), 4, &h) && gesture.Update({14, 0}),
              "Stack child Fixed trailing resize failed");
        gesture.Finish(false);
        Check(cr.FixedSize.x == 94 && cr.Offset == glm::vec2(9, 7) && h.Operations() == 1,
              "Stack resize wrote ignored position or split history");
        auto accepted = f.State();
        auto before = UIEditValues::Capture(f.Control), bad = before;
        bad.Rect->WidthMode = SizeMode::Content;
        cr.WidthMode = SizeMode::Fill;
        cr.Offset = {};
        f.World->RefreshUI();
        accepted = f.State();
        before = UIEditValues::Capture(f.Control);
        bad = before;
        bad.Rect->WidthMode = SizeMode::Content;
        PropertyUI::EditActivity immediate;
        immediate.Changed = immediate.Committed = true;
        Check(!edit.Edit(*f.World, &h, f.Control.GetUUID(), "UI Rect", before, bad, immediate, 0) &&
                  f.State() == accepted,
              "Invalid Inspector dependency changed last valid authored result");
        edit.Finish(*f.World, &h, true);
    }
    {
        HistoryFixture f;
        auto &parent = f.Control.GetComponent<UIRectComponent>();
        parent.LayoutVersion = 2;
        parent.WidthMode = SizeMode::Content;
        parent.FixedSize.y = 80;
        parent.AnchorMin.x = parent.AnchorMax.x = parent.Pivot.x = .5f;
        f.Control.GetComponent<TextComponent>().TextString = "";
        auto child = f.World->CreateEntity("Reflow edge child");
        f.World->Reparent(child, f.Control, TransformPolicy::KeepLocal);
        auto &r = child.AddComponent<UIRectComponent>();
        r.LayoutVersion = 2;
        r.FixedSize = {80, 30};
        f.World->RefreshUI();
        DocumentHistory h;
        h.Bind(f.World->GetIdentity());
        auto before = f.State();
        UIRectEditSession gesture;
        Check(gesture.Begin(f.World, child.GetUUID(), 4, &h) && !gesture.Update({20, 0}) &&
                  f.State() == before,
              "Reflowing parent moved the opposite edge or retained an invalid sample");
        gesture.Finish(false);
        Check(!h.CanUndo() && !h.Active(), "Rejected computed-frame gesture entered history");
        f.Control.GetComponent<UIRectComponent>().Pivot.x = 0;
        f.World->RefreshUI();
        Check(gesture.Begin(f.World, child.GetUUID(), 4, &h) && gesture.Update({20, 0}) &&
                  gesture.Update({30, 0}),
              "Stable-origin Content parent prevented a meaningful Fixed resize");
        gesture.Finish(false);
        Check(r.FixedSize.x == 110 && h.Operations() == 1 && h.Undo(*f.World) &&
                  r.FixedSize.x == 80,
              "Content-parent gesture did not remain one reversible operation");
    }
    std::cout << "PASS: native conversion preview/choice/atomic history/responsive "
                 "roundtrip/isolation; measured Inspector and complete Fixed/owned-axis gestures\n";
}
} // namespace Hazel
