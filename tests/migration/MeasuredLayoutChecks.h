#include "Hazel/UI/MeasuredLayout.h"
static void MeasuredLayoutChecks()
{
    auto node = [](uint64_t id)
    {
        MeasureNode n;
        n.ID = id;
        n.Name = "layout";

        n.Layout.FixedSize = {100, 80};
        return n;
    };
    auto empty = [](size_t, float) { return glm::vec2(0); };
    MeasuredLayout layout;
    std::vector<MeasureNode> nodes{node(10), node(11), node(12)};
    nodes[0].Container = true;
    nodes[0].Flow.Gap = 5;
    nodes[0].Layout.Padding = glm::vec4(5);
    nodes[0].Children = {1, 2};
    for (int i = 1; i < 3; ++i)
    {
        nodes[i].Parent = 0;
        nodes[i].Layout.WidthMode = SizeMode::Fill;
    }
    nodes[1].Layout.FixedSize.y = 10;
    nodes[2].Layout.HeightMode = SizeMode::Fill;
    nodes[2].Layout.MinimumSize.y = 20;
    nodes[2].Layout.HasMaximum = {true, true};
    nodes[2].Layout.MaximumSize = {60, 40};
    layout.Resolve(nodes, {{}, {500, 500}}, empty);
    auto r = layout.Results();
    Check(r[1].Bounds.Size == glm::vec2(90, 10) && r[2].Bounds.Size == glm::vec2(60, 40),
          "Stack min/max or cross Fill failed");
    Check(r[2].Bounds.Position == glm::vec2(5, 20) && !r[0].Overflow,
          "Stack allocated gaps/padding incorrectly");
    auto previous = r;
    layout.Resolve(nodes, {{}, {500, 500}}, empty);
    Check(layout.Results()[2].Bounds.Position == previous[2].Bounds.Position &&
              layout.Results()[2].Bounds.Size == previous[2].Bounds.Size,
          "Repeated measure/arrange was nondeterministic");
    nodes = {node(20), node(21), node(22), node(23)};
    nodes[0].Children = {1, 2, 3};
    nodes[0].Container = true;
    nodes[0].Flow.Direction = Axis::Horizontal;
    nodes[0].Flow.Gap = 0;
    for (int i = 1; i < 4; ++i)
    {
        nodes[i].Parent = 0;
        nodes[i].Layout.WidthMode = SizeMode::Fill;
        nodes[i].Layout.MinimumSize.x = 10;
    }
    nodes[1].Layout.HasMaximum.x = true;
    nodes[1].Layout.MaximumSize.x = 20;
    nodes[2].Weight = 2;
    layout.Resolve(nodes, {{}, {500, 500}}, empty);
    r = layout.Results();
    Check(Same(r[1].Bounds.Size.x, 20) && Same(r[2].Bounds.Size.x, 50) &&
              Same(r[3].Bounds.Size.x, 30),
          "Weighted Fill did not redistribute saturated maxima");
    nodes[1].Weight = 1e30f;
    layout.Resolve(nodes, {{}, {500, 500}}, empty);
    Check(Same(layout.Results()[1].Bounds.Size.x, 20) &&
              Same(layout.Results()[2].Bounds.Size.x, 50) &&
              Same(layout.Results()[3].Bounds.Size.x, 30),
          "Capped large weight erased the remaining Fill weights");
    nodes[1].Weight = 1;
    nodes[1].Visible = false;
    layout.Resolve(nodes, {{}, {500, 500}}, empty);
    Check(layout.Results()[1].Bounds.Size == glm::vec2(0) &&
              Same(layout.Results()[2].Bounds.Size.x, 63.33333f),
          "Hidden child consumed Fill space");
    nodes = {node(30), node(31)};
    nodes[0].Children = {1};
    nodes[0].Layout.HeightMode = SizeMode::Content;
    nodes[0].Layout.Padding = glm::vec4(5);
    nodes[0].Container = true;
    nodes[1].Parent = 0;
    nodes[1].Layout.WidthMode = SizeMode::Fill;
    nodes[1].Layout.HeightMode = SizeMode::Content;
    auto text = [](size_t index, float width)
    {
        return index ? glm::vec2(200, width < 0 ? 20 : 20 * std::ceil(200 / std::max(1.f, width)))
                     : glm::vec2(0);
    };
    layout.Resolve(nodes, {{}, {500, 500}}, text);
    r = layout.Results();
    Check(r[1].Bounds.Size == glm::vec2(90, 60) && r[0].Bounds.Size.y == 70,
          "Constrained child height did not measure before Content stack height");
    nodes[0].Layout.WidthMode = SizeMode::Content;
    bool path = false;
    try
    {
        layout.Resolve(nodes, {{}, {500, 500}}, text);
    }
    catch (const std::invalid_argument &error)
    {
        auto message = std::string(error.what());
        path = message.find("layout#30.width") != std::string::npos &&
               message.find("layout#31.width") != std::string::npos;
    }
    Check(path, "Indefinite Content/Fill dependency omitted the entity/axis path");
    nodes = {node(40)};
    nodes[0].Layout.WidthMode = nodes[0].Layout.HeightMode = SizeMode::Content;
    nodes[0].Layout.Padding = {3, 4, 5, 6};
    layout.Resolve(nodes, {{}, {0, 0}}, empty);
    Check(layout.Results()[0].Bounds.Size == glm::vec2(8, 10) && layout.Results()[0].Overflow,
          "Empty Content or zero-space minimum policy failed");
    nodes[0].Layout.Padding = {};
    nodes[0].Layout.HasMaximum.x = true;
    nodes[0].Layout.MaximumSize.x = 120;
    layout.Resolve(nodes, {{}, {500, 500}},
                   [](size_t, float width) { return glm::vec2(200, width < 0 ? 20 : 40); });
    Check(layout.Results()[0].Bounds.Size == glm::vec2(120, 40),
          "Content maximum did not constrain wrapped height measurement");
    nodes = {node(50), node(51)};
    nodes[0].Children = {1};
    nodes[0].Layout.WidthMode = SizeMode::Content;
    nodes[1].Parent = 0;
    nodes[1].Layout.Anchor.x = nodes[1].Layout.Anchor.x = .5f;
    Reject([&] { layout.Resolve(nodes, {{}, {500, 500}}, empty); });
    nodes = {node(60)};
    nodes[0].Layout.WidthMode = SizeMode::Fill;
    nodes[0].Layout.MinimumSize.x = 20;
    layout.Resolve(nodes, {{}, {0, 100}}, empty);
    Check(layout.Results()[0].Bounds.Size.x == 20 && layout.Results()[0].Overflow,
          "Fill silently violated its minimum");
    nodes[0].Layout.HasMaximum.x = true;
    nodes[0].Layout.MaximumSize.x = 10;
    Reject([&] { layout.Resolve(nodes, {{}, {100, 100}}, empty); });
    auto config = node(70).Layout;
    config.Pivot = {.3f, .7f};
    config.Offset = {20, 30};
    Rect parent{{}, {300, 200}}, start{{-10, -26}, {100, 80}};
    auto edited = EditMeasuredRect(parent, start, config, {15, 0}, 1);
    Check(edited.FixedSize.x == 85 && Same(edited.Offset.x, 30.5f),
          "Measured edge edit failed to preserve the opposite edge");
    config.WidthMode = SizeMode::Content;
    Reject([&] { EditMeasuredRect(parent, start, config, {15, 0}, 1); });
    std::cout
        << "PASS: measured Fixed/Fill/Content, weighted capped stacks, "
           "constraints/overflow/empty/hidden, dependency paths and resolved-edge ownership\n";
}
