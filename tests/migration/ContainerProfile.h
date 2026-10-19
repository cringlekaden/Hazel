// Optional CPU-only layout qualification in the normal runtime integration binary.
// Run Release without a concurrent build. No GPU duration is inferred here.
static void ProfileContainers()
{
    using Clock=std::chrono::steady_clock;
    for(auto kind:{ContainerKind::Grid,ContainerKind::Wrap})for(int count:{64,512,2048}){
        std::ostringstream text;text<<"SceneVersion: 2\nScene: Container CPU profile\nEntities:\n- Entity: 1\n  UICanvasComponent: {ReferenceSize: [1280, 720], SafeInsets: [0, 0, 0, 0]}\n- Entity: 2\n  Relationship: {Parent: 1, Order: 0}\n  UIRectComponent: {LayoutVersion: 3, FixedSize: [1248, 688]}\n  UILayoutComponent:\n    Kind: "<<int(kind)<<"\n    Direction: 0\n    Gap: 2\n    LineGap: 3\n";
        if(kind==ContainerKind::Grid){text<<"    GridGap: [2, 3]\n    Columns: [";for(int i=0;i<32;++i)text<<(i?", ":"")<<"{Mode: 1, Value: 1}";text<<"]\n    Rows: [";for(int i=0;i<(count+31)/32;++i)text<<(i?", ":"")<<"{Mode: 0, Value: 12}";text<<"]\n";}
        for(int i=0;i<count;++i)text<<"- Entity: "<<i+3<<"\n  Relationship: {Parent: 2, Order: "<<i<<"}\n  UIRectComponent: {LayoutVersion: 3, FixedSize: [36, 10]}\n";
        auto scene=CreateRef<Scene>();Check(SceneSerializer(scene).DeserializeText(text.str()),"Container profile import failed");scene->OnViewportResize(1280,720);
        RuntimeUI ui(*scene);for(int i=0;i<20;++i)ui.Resolve({1280,720});auto generation=ui.Snapshot().Generation;
        std::vector<double> idle(200),changed(100);uint64_t idleAlloc=0,changedAlloc=0;
        for(size_t i=0;i<idle.size();++i){s_Allocations=0;s_CountAllocations=true;auto start=Clock::now();ui.Resolve({1280,720});auto end=Clock::now();s_CountAllocations=false;idleAlloc+=s_Allocations;idle[i]=std::chrono::duration<double,std::milli>(end-start).count();}
        Check(ui.Snapshot().Generation==generation,"Idle container rebuilt generation");
        auto root=scene->GetEntityByUUID(2);
        for(size_t i=0;i<changed.size();++i){root.GetComponent<UIRectComponent>().FixedSize.x=1248-float(i%2)*100;s_Allocations=0;s_CountAllocations=true;auto start=Clock::now();ui.Resolve({1280,720});auto end=Clock::now();s_CountAllocations=false;changedAlloc+=s_Allocations;changed[i]=std::chrono::duration<double,std::milli>(end-start).count();}
        std::sort(idle.begin(),idle.end());std::sort(changed.begin(),changed.end());
        std::cout<<std::fixed<<std::setprecision(4)<<"LAYOUT_PROFILE "<<(kind==ContainerKind::Grid?"grid":"wrap")<<" children="<<count<<" unchanged_cpu_ms="<<idle[idle.size()/2]<<"/"<<idle[idle.size()*95/100]<<" changed_cpu_ms="<<changed[changed.size()/2]<<"/"<<changed[changed.size()*95/100]<<" idle_cpp_new="<<idleAlloc<<" changed_cpp_new="<<changedAlloc<<" samples="<<idle.size()<<"/"<<changed.size()<<"\n";
    }
    std::cout<<"PASS: CPU container profiles with idle generation reuse and C++ allocation counts; no draw/GPU work measured\n";
}
