#include "Hazel/Text/TextEditing.h"
struct TestTextClipboard final : Hazel::Text::Clipboard
{
    std::string Value;
    bool Available = true;
    bool Read(std::string &value, size_t limit, std::string &error) override
    {
        if (!Available || Value.size() > limit)
        {
            error = "Test clipboard unavailable/oversized";
            return false;
        }
        value = Value;
        return true;
    }
    bool Write(const std::string &value, std::string &error) override
    {
        if (!Available)
        {
            error = "Test clipboard unavailable";
            return false;
        }
        Value = value;
        return true;
    }
};
static void TextEditingChecks(const std::filesystem::path &root)
{
    using namespace Hazel::Text;
    using C = EditCommand;
    EditBuffer b(u8"a\u0301", {});
    b.Point({2}, false);
    Check(b.Graphemes() == 1, "Combining sequence split into edit positions");
    b.Apply({C::Backspace});
    Check(b.Value().empty(), "Backspace split an extended grapheme");
    b.Apply({C::Undo});
    Check(b.Value() == u8"a\u0301", "Local undo did not restore bytes");
    b.Reset(u8"\ufeff\U0001f600x", {});
    Check(b.Byte(0) == 3 && b.UTF16(0) == 1 && b.UTF16(1) == 3 && b.Graphemes() == 2,
          "Editing BOM/UTF-16 mapping differs from source contract");
    b.Point({1}, false);
    b.Apply({C::Backspace});
    Check(b.Value() == u8"\ufeffx", "Astral deletion corrupted source bytes");
    b.Reset("", {});
    for (int i = 0; i < 8; ++i)
        b.Apply({C::Insert, "a"});
    Check(b.UndoCount() == 1, "Continuous typing did not coalesce");
    b.Apply({C::Undo});
    Check(b.Value().empty(), "Typing undo did not restore start");
    b.Apply({C::Redo});
    Check(b.Value() == "aaaaaaaa", "Text redo failed");
    b.Point({0}, false);
    b.Apply({C::Insert, "b"});
    b.Apply({C::Redo});
    Check(b.Value() == "baaaaaaaa", "New edit retained a redo branch");
    TestTextClipboard clip;
    b.Apply({C::All});
    b.Apply({C::Copy}, nullptr, &clip);
    Check(clip.Value == b.Value(), "Copy lost selected content");
    clip.Value = "first\r\nsecond";
    b.Apply({C::Paste}, nullptr, &clip);
    Check(b.Value() == "first second", "Single-line paste normalization failed");
    auto accepted = b.Value();
    clip.Value = std::string("bad\0payload", 11);
    bool rejected = false;
    try
    {
        b.Apply({C::Paste}, nullptr, &clip);
    }
    catch (const std::exception &)
    {
        rejected = true;
    }
    Check(rejected && b.Value() == accepted, "Malformed clipboard partially changed text");
    EditLimits tiny;
    tiny.Bytes = 8;
    tiny.Graphemes = 2;
    b.Reset("a", tiny);
    rejected = false;
    try
    {
        b.Apply({C::Insert, "abc"});
    }
    catch (const std::exception &)
    {
        rejected = true;
    }
    Check(rejected && b.Value() == "a", "Grapheme limit accepted a partial insertion");
    tiny.Multiline = true;
    tiny.Bytes = 32;
    tiny.Graphemes = 16;
    tiny.Lines = 2;
    b.Reset("a\nb", tiny);
    b.Apply({C::All});
    rejected = false;
    try
    {
        b.Apply({C::Insert, "a\nb\nc"});
    }
    catch (const std::exception &)
    {
        rejected = true;
    }
    Check(rejected && b.Value() == "a\nb", "Explicit line limit corrupted text");
    EditLimits ro;
    ro.ReadOnly = true;
    b.Reset("read only", ro);
    b.Apply({C::All});
    b.Apply({C::Cut}, nullptr, &clip);
    Check(b.Value() == "read only" && clip.Value == "read only", "Read-only selection/copy semantics failed");
    b.Reset("", {});
    for (int i = 0; i < 70; ++i)
    {
        b.BreakUndo();
        b.Apply({C::Insert, "x"});
    }
    Check(b.UndoCount() <= EditBuffer::HistoryCount && b.RetainedHistoryBytes() <= EditBuffer::HistoryBytes,
          "Local history cap exceeded");
    b.Reset("one two",{});b.Apply({C::Right,{},false,true});Check(b.Selection().Caret.Scalar==3,"Word movement skipped the independent word boundary");b.Apply({C::Right,{},true,true});Check(b.Selected()==" ","Shift-word selection lost the whitespace segment");b.Reset("can't stop",{});b.Apply({C::Right,{},false,true});Check(b.Selection().Caret.Scalar==5,"Word movement split an apostrophe word");
    Service service;
    auto face = service.CreateFace(Read(root / "DejaVuSans.ttf"), "edit-reference");
    Options options;
    options.EmSize = 1 / face->HeightEm();
    auto layout = service.Arrange("office", face, options);
    EditGeometry geometry;
    geometry.Build(layout);
    std::vector<bool> edges(7);
    for (auto stop : geometry.Stops())
        if (stop.Source.Scalar < 7)
            edges[stop.Source.Scalar] = true;
    Check(std::all_of(edges.begin(), edges.end(), [](bool v) { return v; }),
          "Ligature caret fallback omitted grapheme boundaries");
    layout = service.Arrange(u8"a\u0301", face, options);
    geometry.Build(layout);
    for (auto stop : geometry.Stops())
        Check(stop.Source.Scalar != 1, "Caret entered a combining sequence");
    layout = service.Arrange(u8"abc אבג def", face, options);
    geometry.Build(layout);
    Check(geometry.Selection(0, 5).size() >= 2,
          "Mixed-bidi selection was approximated as one bounding rectangle");
    layout = service.Arrange(u8"אבג", face, options);
    geometry.Build(layout);
    float preferred = -1;
    Check(geometry.Move({0}, -1, 0, preferred).Scalar == 1 && geometry.Move({0}, 1, 0, preferred).Scalar == 0,
          "RTL Left/Right did not follow visual positions");
    b.Reset(u8"سلام",{});layout=service.Arrange(b.Value(),face,options);geometry.Build(layout);b.Point({3},false);b.Apply({C::Backspace},&geometry);Check(b.Value()==u8"سلم","Arabic deletion split a logical grapheme");auto joined=service.Arrange(b.Value(),face,options);bool reshaped=layout->Glyphs.size()!=joined->Glyphs.size();for(size_t i=0;i<std::min(layout->Glyphs.size(),joined->Glyphs.size());++i)reshaped|=layout->Glyphs[i].ID!=joined->Glyphs[i].ID;Check(reshaped,"Arabic context did not reshape after deleting a ligature participant");
    layout=service.Arrange("iiii\nWWWW\niiii",face,options);geometry.Build(layout);preferred=-1;auto wide=geometry.Move({2},0,1,preferred);auto restored=geometry.Move(wide,0,1,preferred);Check(restored.Scalar==12,"Vertical movement lost its preferred X across unequal advances");
    layout = service.Arrange("a\n" + std::string(2048, 'W'), face, options);
    geometry.Build(layout);
    auto firstLine = geometry.Locate({0});
    Check(geometry.Hit({100000, firstLine.Point.y + firstLine.Height * .99f}).Line == 0,
          "Outside pointer hit chose a distant long line over the nearest line");
    options.Width = 1.6f;
    layout = service.Arrange("word word word", face, options);
    geometry.Build(layout);
    Check(layout->Lines.size() > 1, "Wrap fixture did not wrap");
    bool affinity = false;
    for (auto a : geometry.Stops())
        for (auto z : geometry.Stops())
            affinity |= a.Source.Scalar == z.Source.Scalar && a.Source.Line != z.Source.Line;
    Check(affinity, "Soft-wrap source boundary lost line affinity");
    const auto calls = layout->ShapeCalls;
    preferred = -1;
    for (int i = 0; i < 100; ++i)
        geometry.Move({}, 1, 0, preferred);
    Check(layout->ShapeCalls == calls, "Caret movement reshaped text");
    std::cout << "PASS: CPU extended-grapheme editing/source maps, word/visual geometry, ligature fallback, "
                 "bidi selection spans, clipboard normalization and bounded local undo\n";
}
