#pragma once
namespace Hazel
{
static bool SameBounds(GameUI::Rect a, GameUI::Rect b)
{
    return glm::length(a.Position - b.Position) < .02f && glm::length(a.Size - b.Size) < .02f;
}
static void EditorLayoutChecks()
{
    using namespace GameUI;
    {
        HistoryFixture f;
        auto &r = f.Control.GetComponent<UIRectComponent>();

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

        parent.WidthMode = SizeMode::Content;
        parent.FixedSize.y = 80;
        parent.Anchor.x = parent.Pivot.x = .5f;
        f.Control.GetComponent<TextComponent>().TextString = "";
        auto child = f.World->CreateEntity("Reflow edge child");
        f.World->Reparent(child, f.Control, TransformPolicy::KeepLocal);
        auto &r = child.AddComponent<UIRectComponent>();

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
    {
        HistoryFixture f;f.Control.AddComponent<UILayoutComponent>();f.World->RefreshUI();
        DocumentHistory h;h.Bind(f.World->GetIdentity());auto saved=f.State();h.MarkSaved(saved);
        UIInspectorEditing edit;PropertyUI::EditActivity activity;activity.Changed=activity.Committed=true;
        auto before=UIEditValues::Capture(f.Control),next=before;
        next.Flow->Kind=ContainerKind::Grid;next.Flow->Columns={{TrackMode::Fill,1,0,100,true},{TrackMode::Fixed,40}};next.Flow->Rows={{TrackMode::Fixed,80}};next.Flow->GridGap={4,6};
        Check(edit.Edit(*f.World,&h,f.Control.GetUUID(),"Grid tracks",before,next,activity,0),edit.Error());
        auto changed=f.State();Check(h.Operations()==1&&changed!=saved&&h.Undo(*f.World)&&f.State()==saved&&h.Redo(*f.World)&&f.State()==changed,"Grid track list/mode edit failed coherent history");
        before=UIEditValues::Capture(f.Control);next=before;next.Flow->Columns[0].Maximum=-1;
        Check(!edit.Edit(*f.World,&h,f.Control.GetUUID(),"Invalid track",before,next,activity,0)&&f.State()==changed,"Invalid track edit lost last valid state");edit.Finish(*f.World,&h,true);
        auto other=CreateRef<Scene>();auto otherState=SceneSerializer(other).CaptureAuthoredState();Check(!h.Undo(*other)&&SceneSerializer(other).CaptureAuthoredState()==otherState,"Grid edit applied to another document");
        auto serialized=SceneSerializer(f.World).SerializeText();auto loaded=CreateRef<Scene>();Check(SceneSerializer(loaded).DeserializeText(serialized)&&SceneSerializer(loaded).CaptureAuthoredState()==changed,"Grid Inspector save/reopen lost values");
    }
    {
        HistoryFixture f;f.Control.AddComponent<UILayoutComponent>();f.World->RefreshUI();DocumentHistory h;h.Bind(f.World->GetIdentity());
        const auto initial=f.State();auto before=UIEditValues::Capture(f.Control),next=before;next.Flow->Kind=ContainerKind::Wrap;next.Flow->Direction=Axis::Horizontal;next.Flow->LineGap=18;next.Flow->LineAlign=Align::Center;
        UIInspectorEditing edit;PropertyUI::EditActivity committed;committed.Changed=committed.Committed=true;
        Check(edit.Edit(*f.World,&h,f.Control.GetUUID(),"Wrap settings",before,next,committed,0),edit.Error());auto accepted=f.State();
        Check(h.Operations()==1&&h.Undo(*f.World)&&f.State()==initial&&h.Redo(*f.World)&&f.State()==accepted,"Wrap mode/line configuration failed shared history");
        before=UIEditValues::Capture(f.Control);next=before;next.Rect->WidthMode=SizeMode::Content;
        Check(!edit.Edit(*f.World,&h,f.Control.GetUUID(),"Indefinite wrap",before,next,committed,0)&&f.State()==accepted,"Indefinite wrap edit replaced valid geometry");edit.Finish(*f.World,&h,true);
    }
    std::cout << "PASS: canonical grid tracks/atomic history/save-reopen/isolation; "
                 "measured Inspector and complete Fixed/owned-axis gestures\n";
}
} // namespace Hazel
