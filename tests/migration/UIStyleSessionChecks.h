static void UIStyleSessionChecks(const Ref<Project> &project)
{
    auto authored = TextEntryFixture();
    authored->GetEntityByUUID(900).AddComponent<ScriptComponent>().ClassName =
        "Migration.StyleOwnerProbe";
    authored->GetEntityByUUID(500).AddComponent<UIAppearanceComponent>().Overrides =
        UIStyleBit(UIStyleGroup::Foreground);
    auto original = SceneSerializer(authored).CaptureAuthoredState();
    RuntimeSession session;
    session.Resize(400, 300);
    for (int i = 0; i < 3; ++i)
    {
        session.Start(project, authored);
        auto live = session.GetScene();
        auto owner = ScriptEngine::GetEntityScriptInstance(900);
        Check(owner && owner->GetFieldValue<bool>("Ready"),
              "Managed style selection/reset roundtrip failed");
        Check(live->GetEntityByUUID(500).GetComponent<UIAppearanceComponent>().Overrides == 0,
              "Managed reset did not remove presence");
        auto value = live->GetUITextEntry(500);
        live->OnUpdateRuntime(0);
        Check(owner->GetFieldValue<bool>("Changed") && live->GetUIStyle(500).Variant == "Control" &&
                  live->GetUITextEntry(500) == value,
              "Managed style assignment changed text/runtime control state");
        session.Stop();
        Check(SceneSerializer(authored).CaptureAuthoredState() == original,
              "Styled Play/Stop leaked runtime presentation into authoring");
    }
    std::cout << "PASS: managed typed style assignment/get/reset, silent state isolation and three "
                 "Play/Stop cycles\n";
}
