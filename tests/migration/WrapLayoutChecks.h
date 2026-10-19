#pragma once
static void WrapLayoutChecks()
{
    MeasuredLayout layout;auto empty=[](size_t,float){return glm::vec2(0);};
    std::vector<MeasureNode> n(5);
    for(size_t i=0;i<n.size();++i){n[i].ID=200+i;n[i].Name="Wrap";n[i].Layout.FixedSize={45,20};if(i){n[i].Parent=0;n[0].Children.push_back(i);}}
    n[0].Container=true;n[0].Flow.Kind=ContainerKind::Wrap;n[0].Flow.Direction=Axis::Horizontal;n[0].Flow.Gap=10;n[0].Flow.LineGap=7;n[0].Layout.FixedSize.x=100;n[0].Layout.HeightMode=SizeMode::Content;
    layout.Resolve(n,{{},{200,200}},empty);auto r=layout.Results();
    Check(r[1].Line==0&&r[2].Line==0&&r[3].Line==1&&r[4].Line==1&&r[0].Bounds.Size.y==47,"Wrap exact boundary or Content height failed");
    n[0].Layout.FixedSize.x=99;layout.Resolve(n,{{},{200,200}},empty);Check(layout.Results()[4].Line==3&&layout.Results()[0].Bounds.Size.y==101,"Narrow width did not deterministically reflow");
    n[0].Layout.FixedSize.x=100;n[1].Visible=false;layout.Resolve(n,{{},{200,200}},empty);Check(layout.Results()[2].Line==0&&layout.Results()[3].Line==0&&layout.Results()[4].Line==1,"Hidden wrap item retained a gap/slot");n[1].Visible=true;
    n[1].Layout.WidthMode=SizeMode::Fill;n[1].Layout.MinimumSize.x=20;n[1].Layout.HasMaximum.x=true;n[1].Layout.MaximumSize.x=30;
    layout.Resolve(n,{{},{200,200}},empty);Check(layout.Results()[1].Bounds.Size.x==30&&layout.Results()[2].Bounds.Position.x==40,"Per-line Fill/cap allocation failed");
    n[1].Layout.WidthMode=SizeMode::Fixed;n[1].Layout.HasMaximum.x=false;n[1].Layout.FixedSize.x=120;
    layout.Resolve(n,{{},{200,200}},empty);Check(layout.Results()[1].Line==0&&layout.Results()[2].Line==1&&layout.Results()[0].Overflow,"Oversized first item did not retain its own overflowing line");
    n[1].Layout.FixedSize.x=45;n[1].Layout.HeightMode=SizeMode::Fill;n[1].Layout.MinimumSize.y=5;n[2].Layout.FixedSize.y=30;
    layout.Resolve(n,{{},{200,200}},empty);Check(layout.Results()[1].Bounds.Size.y==30,"Cross Fill did not use its line thickness");
    n[0].Layout.WidthMode=SizeMode::Content;Reject([&]{layout.Resolve(n,{{},{200,200}},empty);});n[0].Layout.WidthMode=SizeMode::Fixed;
    n[0].Flow.Direction=Axis::Vertical;n[0].Layout.HeightMode=SizeMode::Fixed;n[0].Layout.FixedSize={200,50};n[0].Layout.WidthMode=SizeMode::Content;
    for(size_t i=1;i<n.size();++i){n[i].Layout.WidthMode=SizeMode::Fixed;n[i].Layout.HeightMode=SizeMode::Fixed;n[i].Layout.FixedSize={20,20};n[i].Layout.MinimumSize={};}
    layout.Resolve(n,{{},{200,200}},empty);Check(layout.Results()[3].Line==1&&layout.Results()[0].Bounds.Size.x==47,"Vertical wrap columns or Content width failed");
    auto previous=layout.Results();layout.Resolve(n,{{},{200,200}},empty);Check(previous[4].Bounds.Position==layout.Results()[4].Bounds.Position,"Repeated wrap arrangement changed geometry");
    n[1].Layout.WidthMode=SizeMode::Fill;n[1].Layout.HeightMode=SizeMode::Content;
    Reject([&]{layout.Resolve(n,{{},{200,200}},[](size_t,float width){return glm::vec2(80,width<0?20:40);});});
    std::cout<<"PASS: horizontal/vertical wrap, exact breaks, capped Fill, line thickness, hidden/oversized items and dependency rejection\n";
}
