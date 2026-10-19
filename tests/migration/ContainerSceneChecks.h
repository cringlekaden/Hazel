#pragma once
static void ContainerSceneChecks(const std::filesystem::path &root)
{
    auto scene=CreateRef<Scene>();scene->SetAssets(CreateRef<ProjectAssets>(root));scene->OnViewportResize(320,240);
    auto canvas=scene->CreateEntity("Container canvas");canvas.AddComponent<UICanvasComponent>().ReferenceSize={320,240};canvas.GetComponent<UICanvasComponent>().SafeInsets={};
    auto grid=scene->CreateEntity("Icon grid");scene->Reparent(grid,canvas,TransformPolicy::KeepLocal);grid.AddComponent<UIRectComponent>().FixedSize={280,160};
    auto &container=grid.AddComponent<UILayoutComponent>();container.Kind=ContainerKind::Grid;container.Columns={{TrackMode::Fill,1},{TrackMode::Fill,1}};container.Rows={{TrackMode::Fixed,60},{TrackMode::Fixed,60}};container.GridGap={8,10};
    std::vector<uint64_t> children;
    for(int i=0;i<3;++i){auto child=scene->CreateEntity("Icon "+std::to_string(i));children.push_back(child.GetUUID());scene->Reparent(child,grid,TransformPolicy::KeepLocal);auto &r=child.AddComponent<UIRectComponent>();r.WidthMode=r.HeightMode=SizeMode::Fill;child.AddComponent<SpriteRendererComponent>();child.AddComponent<UIButtonComponent>().Action="icon";}
    scene->RefreshUI();auto &snapshot=scene->GetUISnapshot();auto first=snapshot.Find(children[0]);
    Check(first&&first->ContainerOwned&&first->Bounds.Size==glm::vec2(136,60)&&snapshot.Guides.size()==4,"Grid scene lost cell/track snapshot geometry");
    auto generation=snapshot.Generation;for(int i=0;i<4;++i)scene->RefreshUI();Check(scene->GetUISnapshot().Generation==generation,"Unchanged grid generation rebuilt");
    InputFrame input;input.Enabled=true;scene->UpdateUIInput(input);auto inputGeneration=scene->GetUIDiagnostics().InputGeneration;
    grid.GetComponent<UILayoutComponent>().Columns[0].Value=2;scene->RefreshUI();
    Check(scene->GetUISnapshot().Generation>generation&&scene->GetUISnapshot().Find(children[0])->Bounds.Size.x>136&&scene->GetUIDiagnostics().InputGeneration==inputGeneration,"Native track change missed invalidation or replayed input");
    auto authored=SceneSerializer(scene,root).CaptureAuthoredState();auto proposal=grid.GetComponent<UILayoutComponent>();proposal.Rows.resize(1);
    bool rejected=false;try{scene->SetUILayout(grid.GetUUID(),proposal);}catch(const std::invalid_argument &){rejected=true;}
    Check(rejected&&SceneSerializer(scene,root).CaptureAuthoredState()==authored,"Invalid track count partially published runtime mutation");
    scene->SetUIGridCell(children[0],1,1);scene->RefreshUI();Check(scene->GetUISnapshot().Find(children[0])->GridRow==1,"Runtime explicit cell did not arrange");
    authored=SceneSerializer(scene,root).CaptureAuthoredState();rejected=false;try{scene->SetUIGridCell(children[1],1,1);}catch(const std::invalid_argument &){rejected=true;}
    Check(rejected&&SceneSerializer(scene,root).CaptureAuthoredState()==authored,"Occupied cell mutated valid authored state");
    auto yaml=SceneSerializer(scene,root).SerializeText();auto loaded=CreateRef<Scene>();
    Check(SceneSerializer(loaded,root).DeserializeText(yaml)&&SceneSerializer(loaded,root).CaptureAuthoredState()==authored,"Grid scene roundtrip lost track/placement values");
    Check(SceneSerializer(Scene::Copy(scene),root).CaptureAuthoredState()==authored,"Grid scene clone changed authored fields");
    Prefab::Save(root,"grid.hprefab",scene,grid);auto instance=Prefab::Instantiate(root,"grid.hprefab",*loaded);
    Check(instance.GetComponent<UILayoutComponent>().Columns.size()==2&&instance.GetComponent<UILayoutComponent>().Columns[0].Value==2,"Grid prefab lost track definitions");
    auto unsupported=yaml;auto at=unsupported.find("LayoutVersion: 3");unsupported.replace(at,16,"LayoutVersion: 2");auto before=SceneSerializer(scene,root).CaptureAuthoredState();
    Check(!SceneSerializer(scene,root).DeserializeText(unsupported)&&SceneSerializer(scene,root).CaptureAuthoredState()==before,"Unsupported schema partially replaced a document");
    std::cout<<"PASS: scene grid generation reuse/native writes/atomic mutation/cell eligibility and schema-copy-prefab roundtrip\n";
}
