#pragma once
static void GridLayoutChecks()
{
    MeasuredLayout layout;
    auto intrinsic=[](size_t,float){return glm::vec2(0);};
    std::vector<MeasureNode> n(5);
    for(size_t i=0;i<n.size();++i){n[i].ID=100+i;n[i].Name="Grid";n[i].Layout.FixedSize={40,20};if(i){n[i].Parent=0;n[0].Children.push_back(i);n[i].Layout.WidthMode=SizeMode::Fill;}}
    n[0].Container=true;n[0].Flow.Kind=ContainerKind::Grid;n[0].Layout.FixedSize={320,140};n[0].Layout.Padding=glm::vec4(10);
    n[0].Flow.Columns={{TrackMode::Fixed,80},{TrackMode::Fill,1,20,60,true},{TrackMode::Fill,2,20}};
    n[0].Flow.Rows={{TrackMode::Fixed,40},{TrackMode::Fill,1}};n[0].Flow.GridGap={10,8};
    n[1].Layout.GridRow=1;n[1].Layout.GridColumn=1;n[1].Layout.ColumnSpan=2;
    n[2].Visible=false;
    layout.Resolve(n,{{},{500,500}},intrinsic);auto r=layout.Results();
    Check(r[1].Row==1&&r[1].Column==1&&r[2].Row==0&&r[2].Column==0&&r[3].Column==1&&r[4].Column==2,"Grid explicit/auto/hidden placement changed");
    Check(r[1].Bounds.Size.x==210&&r[3].Bounds.Size.x==60&&r[4].Bounds.Size.x==140,"Grid weighted cap/span allocation failed");
    Check(r[1].Bounds.Position==glm::vec2(100,58)&&r[1].Slot.Size==glm::vec2(210,72),"Grid gap/padding/cell geometry failed");
    auto previous=r;layout.Resolve(n,{{},{500,500}},intrinsic);Check(layout.Results()[4].Bounds.Position==previous[4].Bounds.Position,"Grid arrangement not deterministic");
    n[4].Layout.GridRow=1;n[4].Layout.GridColumn=2;Reject([&]{layout.Resolve(n,{{},{500,500}},intrinsic);});
    n[4].Layout.GridRow=n[4].Layout.GridColumn=-1;n[4].Layout.ColumnSpan=64;Reject([&]{layout.Resolve(n,{{},{500,500}},intrinsic);});n[4].Layout.ColumnSpan=1;
    n[0].Layout.WidthMode=SizeMode::Content;Reject([&]{layout.Resolve(n,{{},{500,500}},intrinsic);});
    n[0].Flow.Columns={{TrackMode::Fixed,80},{TrackMode::Fixed,60},{TrackMode::Fixed,140}};
    layout.Resolve(n,{{},{0,0}},intrinsic);Check(layout.Results()[0].Bounds.Size.x==320&&layout.Results()[0].Overflow,"Content grid or zero external constraints failed");
    n[3].Layout.MinimumSize.x=90;layout.Resolve(n,{{},{500,500}},intrinsic);Check(layout.Results()[3].Bounds.Size.x==90&&layout.Results()[3].Overflow,"Grid silently violated cell child minimum");
    n[0].Flow.Columns.resize(65);Reject([&]{layout.Resolve(n,{{},{500,500}},intrinsic);});
    std::cout<<"PASS: explicit grid tracks, weighted caps, spans, stable hidden reservations, overflow and invalid dependencies\n";
}
