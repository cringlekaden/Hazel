#include "hzpch.h"
#include "TextEditing.h"
#include "Hazel/Utils/UTF8.h"
#include <graphemebreak.h>
#include <wordbreak.h>
#include <algorithm>
#include <cmath>
namespace Hazel::Text
{
namespace
{
void Check(bool ok, const char *message)
{
    if (!ok)
        throw std::invalid_argument(message);
}
} // namespace
std::string EncodeScalar(uint32_t cp)
{
    Check(cp && cp <= 0x10ffff && !(cp >= 0xd800 && cp <= 0xdfff), "Committed text is not a Unicode scalar");
    std::string s;
    if (cp < 0x80)
        s += char(cp);
    else if (cp < 0x800)
    {
        s += char(0xc0 | (cp >> 6));
        s += char(0x80 | (cp & 63));
    }
    else if (cp < 0x10000)
    {
        s += char(0xe0 | (cp >> 12));
        s += char(0x80 | ((cp >> 6) & 63));
        s += char(0x80 | (cp & 63));
    }
    else
    {
        s += char(0xf0 | (cp >> 18));
        s += char(0x80 | ((cp >> 12) & 63));
        s += char(0x80 | ((cp >> 6) & 63));
        s += char(0x80 | (cp & 63));
    }
    return s;
}
std::string EditBuffer::Normalize(const std::string &value, const EditLimits &limits)
{
    Check(limits.Bytes && limits.Bytes <= MaxBytes && limits.Graphemes && limits.Graphemes <= MaxGraphemes &&
              limits.Lines && limits.Lines <= MaxLines,
          "Invalid text-entry limits");
    Check(value.size() <= MaxBytes, "Text input exceeds 16384 bytes");
    std::vector<uint32_t> scalars;
    UTF8::Decode(value, scalars);
    std::string out;
    out.reserve(value.size());
    uint32_t lines = 1;
    // Preserve a leading BOM in stored values and source mappings. It is not an editing scalar.
    if (value.substr(0, 3) == "\xef\xbb\xbf")
        out = "\xef\xbb\xbf";
    for (size_t i = 0; i < scalars.size(); ++i)
    {
        auto cp = scalars[i];
        if (cp == '\r')
        {
            if (i + 1 < scalars.size() && scalars[i + 1] == '\n')
                ++i;
            cp = '\n';
        }
        if (cp == 0x2028 || cp == 0x2029)
            cp = '\n';
        if (cp == '\n')
        {
            if (!limits.Multiline)
                cp = ' ';
            else
                Check(++lines <= limits.Lines, "Text-entry line limit exceeded");
        }
        if (cp == '\t' && !limits.Multiline)
            cp = ' ';
        Check(cp >= 0x20 || cp == '\n' || cp == '\t', "Unsupported text-entry control character");
        out += EncodeScalar(cp);
    }
    Check(out.size() <= limits.Bytes, "Text-entry byte limit exceeded");
    std::vector<uint32_t> decoded;
    UTF8::Decode(out, decoded);
    std::vector<char> breaks(decoded.size());
    if (!decoded.empty())
        set_graphemebreaks_utf32(decoded.data(), decoded.size(), nullptr, breaks.data());
    uint32_t count = decoded.empty() ? 0 : 1;
    for (size_t i = 0; i + 1 < decoded.size(); ++i)
        count += breaks[i] == GRAPHEMEBREAK_BREAK;
    Check(count <= limits.Graphemes, "Text-entry grapheme limit exceeded");
    return out;
}
void EditBuffer::Analyze()
{
    std::vector<size_t> bytes;
    UTF8::Decode(m_Text, m_Scalars, &bytes);
    m_Bytes.assign(bytes.begin(), bytes.end());
    m_UTF16.resize(bytes.size());
    uint32_t u = m_Text.substr(0, 3) == "\xef\xbb\xbf" ? 1 : 0;
    for (size_t i = 0; i < bytes.size(); ++i)
    {
        m_UTF16[i] = u;
        if (i < m_Scalars.size())
            u += m_Scalars[i] > 0xffff ? 2 : 1;
    }
    std::vector<char> g(m_Scalars.size()), w(m_Scalars.size());
    if (!g.empty())
    {
        set_graphemebreaks_utf32(m_Scalars.data(), m_Scalars.size(), nullptr, g.data());
        set_wordbreaks_utf32(m_Scalars.data(), m_Scalars.size(), nullptr, w.data());
    }
    m_Grapheme.assign(bytes.size(), true);
    m_Word.assign(bytes.size(), true);
    for (size_t i = 1; i < m_Scalars.size(); ++i)
    {
        m_Grapheme[i] = g[i - 1] == GRAPHEMEBREAK_BREAK;
        m_Word[i] = m_Grapheme[i] && w[i - 1] == WORDBREAK_BREAK;
    }
}
void EditBuffer::Reset(const std::string &text, const EditLimits &limits)
{
    auto accepted = Normalize(text, limits);
    m_Text = std::move(accepted);
    m_Limits = limits;
    Analyze();
    m_Selection = {};
    m_Undo.clear();
    m_Redo.clear();
    m_Coalescing = false;
    m_Preferred = -1;
}
uint32_t EditBuffer::Byte(uint32_t scalar) const
{
    return m_Bytes.at(scalar);
}
uint32_t EditBuffer::UTF16(uint32_t scalar) const
{
    return m_UTF16.at(scalar);
}
uint32_t EditBuffer::Graphemes() const
{
    return uint32_t(std::count(m_Grapheme.begin(), m_Grapheme.end(), true) - 1);
}
std::string EditBuffer::Selected() const
{
    auto a = std::min(m_Selection.Anchor.Scalar, m_Selection.Caret.Scalar),
         b = std::max(m_Selection.Anchor.Scalar, m_Selection.Caret.Scalar);
    return m_Text.substr(Byte(a), Byte(b) - Byte(a));
}
size_t EditBuffer::RetainedHistoryBytes() const
{
    size_t bytes = 0;
    for (auto &s : m_Undo)
        bytes += sizeof(State) + s.Value.capacity();
    for (auto &s : m_Redo)
        bytes += sizeof(State) + s.Value.capacity();
    return bytes;
}
void EditBuffer::Record(EditCommand command)
{
    bool coalesce = m_Coalescing && m_Last == command &&
                    (command == EditCommand::Insert || command == EditCommand::Backspace ||
                     command == EditCommand::Delete) &&
                    m_Selection.Anchor.Scalar == m_Selection.Caret.Scalar;
    if (!coalesce)
        m_Undo.push_back({m_Text, m_Selection});
    m_Redo.clear();
    while (m_Undo.size() > HistoryCount || RetainedHistoryBytes() > HistoryBytes)
        m_Undo.pop_front();
    m_Coalescing = true;
    m_Last = command;
}
void EditBuffer::Restore(State state)
{
    m_Text = std::move(state.Value);
    m_Selection = state.Selection;
    Analyze();
    m_Preferred = -1;
    m_Coalescing = false;
}
uint32_t EditBuffer::Step(uint32_t at, int direction, bool word) const
{
    auto n = uint32_t(m_Scalars.size());
    do
    {
        if ((direction < 0 && !at) || (direction > 0 && at == n))
            return at;
        at = uint32_t(int(at) + direction);
    } while (!(word ? m_Word[at] : m_Grapheme[at]));
    return at;
}
void EditBuffer::Select(Position p, bool extend)
{
    Check(p.Scalar < m_Grapheme.size() && m_Grapheme[p.Scalar], "Caret is not an extended grapheme boundary");
    m_Selection.Caret = p;
    if (!extend)
        m_Selection.Anchor = p;
}
void EditBuffer::Point(Position p, bool extend)
{
    Select(p, extend);
    m_Preferred = -1;
    BreakUndo();
}
bool EditBuffer::Apply(const EditInput &input, const EditGeometry *geometry, Clipboard *clipboard)
{
    const auto command = input.Command;
    auto before = m_Text;
    auto caret = m_Selection.Caret;
    const uint32_t a = std::min(m_Selection.Anchor.Scalar, caret.Scalar),
                   b = std::max(m_Selection.Anchor.Scalar, caret.Scalar);
    auto move = [&](Position p) {
        Select(p, input.Shift);
        BreakUndo();
    };
    if (command == EditCommand::All)
    {
        m_Selection.Anchor = {};
        m_Selection.Caret = {uint32_t(m_Scalars.size()), 0, true};
        m_Preferred = -1;
        BreakUndo();
        return false;
    }
    if (command == EditCommand::Left || command == EditCommand::Right)
    {
        int d = command == EditCommand::Left ? -1 : 1;
        if (!input.Shift && a != b)
        {
            move({d < 0 ? a : b, caret.Line, d > 0});
            m_Preferred = -1;
            return false;
        }
        if (input.Word || !geometry)
            caret = {Step(caret.Scalar, d, input.Word), 0, d < 0};
        else
            caret = geometry->Move(caret, d, 0, m_Preferred);
        m_Preferred = -1;
        move(caret);
        return false;
    }
    if (command == EditCommand::Up || command == EditCommand::Down)
    {
        if (geometry)
            move(geometry->Move(caret, 0, command == EditCommand::Up ? -1 : 1, m_Preferred));
        return false;
    }
    if (command == EditCommand::Home || command == EditCommand::End)
    {
        bool end = command == EditCommand::End;
        move(input.Word ? Position{end ? uint32_t(m_Scalars.size()) : 0, 0, end}
             : geometry ? geometry->Edge(caret, end)
                        : Position{end ? uint32_t(m_Scalars.size()) : 0, 0, end});
        m_Preferred = -1;
        return false;
    }
    if (command == EditCommand::Copy || command == EditCommand::Cut)
    {
        if (!clipboard)
            throw std::runtime_error("Clipboard unavailable");
        std::string error;
        if (a != b && !clipboard->Write(Selected(), error))
            throw std::runtime_error(error);
        if (command == EditCommand::Copy)
        {
            BreakUndo();
            return false;
        }
    }
    if (m_Limits.ReadOnly)
        return false;
    if (command == EditCommand::Undo || command == EditCommand::Redo)
    {
        auto &from = command == EditCommand::Undo ? m_Undo : m_Redo;
        auto &to = command == EditCommand::Undo ? m_Redo : m_Undo;
        BreakUndo();
        if (from.empty())
            return false;
        to.push_back({m_Text, m_Selection});
        auto state = std::move(from.back());
        from.pop_back();
        while (to.size() > HistoryCount || RetainedHistoryBytes() > HistoryBytes)
            to.pop_front();
        Restore(std::move(state));
        return m_Text != before;
    }
    uint32_t begin = a, end = b;
    std::string insert;
    if (command == EditCommand::Insert)
        insert = input.Text;
    else if (command == EditCommand::Paste)
    {
        if (!clipboard)
            throw std::runtime_error("Clipboard unavailable");
        std::string error;
        if (!clipboard->Read(insert, MaxBytes, error))
            throw std::runtime_error(error);
    }
    else if (command == EditCommand::Enter)
    {
        if (!m_Limits.Multiline)
            return false;
        insert = "\n";
    }
    else if (command == EditCommand::Backspace)
    {
        if (a == b)
            begin = Step(a, -1, input.Word);
    }
    else if (command == EditCommand::Delete)
    {
        if (a == b)
            end = Step(b, 1, input.Word);
    }
    else if (command != EditCommand::Cut)
        return false;
    // A BOM in inserted data is ignored as a transport signature, not inserted midway.
    if (insert.substr(0, 3) == "\xef\xbb\xbf")
        insert.erase(0, 3);
    auto normalized = Normalize(insert, m_Limits);
    Check(normalized.size() <= m_Limits.Bytes - (m_Text.size() - (Byte(end) - Byte(begin))),
          "Text-entry byte limit exceeded");
    auto next = m_Text.substr(0, Byte(begin)) + normalized + m_Text.substr(Byte(end));
    next = Normalize(next, m_Limits);
    if (next == m_Text)
    {
        BreakUndo();
        return false;
    }
    auto position = Byte(begin) + normalized.size();
    Record(command);
    m_Text = std::move(next);
    Analyze();
    auto found = std::lower_bound(m_Bytes.begin(), m_Bytes.end(), position);
    uint32_t index = uint32_t(found - m_Bytes.begin());
    // Insertion may join its neighboring grapheme; choose its next valid edge.
    while (index < m_Grapheme.size() - 1 && !m_Grapheme[index])
        ++index;
    m_Selection.Anchor = m_Selection.Caret = {index, 0, true};
    m_Preferred = -1;
    return true;
}
void EditGeometry::Build(const Ref<const Layout> &layout)
{
    Check(bool(layout), "Editing geometry needs a complete layout");
    m_Layout = layout;
    m_Stops.clear();
    m_Segments.clear();
    const auto &l = *layout;
    float height = l.Font->HeightEm() * l.EmSize;
    for (uint32_t line = 0; line < l.Lines.size(); ++line)
    {
        auto &ln = l.Lines[line];
        float y = ln.Baseline - l.Font->AscenderEm() * l.EmSize;
        auto add = [&](uint32_t scalar, float x, bool upstream) {
            m_Stops.push_back({{scalar, line, upstream}, {x, y}, height});
        };
        if (!ln.RunCount)
        {
            add(ln.Begin, 0, false);
            continue;
        }
        for (uint32_t r = ln.FirstRun; r < ln.FirstRun + ln.RunCount; ++r)
        {
            const auto &run = l.Runs[r];
            for (uint32_t g = run.FirstGlyph; g < run.FirstGlyph + run.GlyphCount;)
            {
                auto begin = l.Glyphs[g].ClusterBegin, end = l.Glyphs[g].ClusterEnd;
                float left = l.Glyphs[g].X, right = left;
                uint32_t next = g;
                while (next < run.FirstGlyph + run.GlyphCount && l.Glyphs[next].ClusterBegin == begin)
                {
                    left = std::min(left, l.Glyphs[next].X);
                    right = std::max(right, l.Glyphs[next].X + l.Glyphs[next].AdvanceX);
                    ++next;
                }
                std::vector<uint32_t> edges;
                for (auto i = begin; i <= end; ++i)
                    if (l.Boundaries.at(i).Grapheme)
                        edges.push_back(i);
                if (edges.size() > 1)
                    for (size_t i = 0; i + 1 < edges.size(); ++i)
                    {
                        float x0 = left + (right - left) * float(i) / float(edges.size() - 1),
                              x1 = left + (right - left) * float(i + 1) / float(edges.size() - 1);
                        if (run.Level & 1)
                        {
                            x0 = right - (x0 - left);
                            x1 = right - (x1 - left);
                        }
                        add(edges[i], x0, false);
                        add(edges[i + 1], x1, true);
                        m_Segments.push_back(
                            {edges[i], edges[i + 1], line, std::min(x0, x1), std::max(x0, x1)});
                    }
                g = next;
            }
        }
        // Zero-advance/format runs still retain their logical line edges.
        bool first = false, last = false;
        for (auto &s : m_Stops)
            if (s.Source.Line == line)
            {
                first |= s.Source.Scalar == ln.Begin;
                last |= s.Source.Scalar == ln.End;
            }
        if (!first)
            add(ln.Begin, 0, false);
        if (!last)
            add(ln.End, ln.Width, true);
    }
    std::stable_sort(m_Stops.begin(), m_Stops.end(), [](const auto &a, const auto &b) {
        return a.Source.Line != b.Source.Line ? a.Source.Line < b.Source.Line : a.Point.x < b.Point.x;
    });
    m_Stops.erase(std::unique(m_Stops.begin(), m_Stops.end(),
                              [](const auto &a, const auto &b) {
                                  return a.Source.Scalar == b.Source.Scalar &&
                                         a.Source.Line == b.Source.Line &&
                                         std::abs(a.Point.x - b.Point.x) < .0001f;
                              }),
                  m_Stops.end());
}
CaretStop EditGeometry::Locate(Position p) const
{
    Check(!m_Stops.empty(), "No caret geometry");
    const CaretStop *best = nullptr;
    for (auto &s : m_Stops)
        if (s.Source.Scalar == p.Scalar)
        {
            if (!best)
                best = &s;
            if (s.Source.Line == p.Line && s.Source.Upstream == p.Upstream)
                return s;
            if (s.Source.Upstream == p.Upstream)
                best = &s;
        }
    return best ? *best : m_Stops.back();
}
Position EditGeometry::Hit(glm::vec2 p) const
{
    Check(!m_Stops.empty(), "No caret geometry");
    const CaretStop *best = &m_Stops.front();
    float distance = INFINITY;
    for (auto &s : m_Stops)
    {
        float dy = p.y < s.Point.y              ? s.Point.y - p.y
                   : p.y > s.Point.y + s.Height ? p.y - s.Point.y - s.Height
                                                : 0;
        float d = dy * 10000 + std::abs(p.x - s.Point.x);
        if (d < distance)
        {
            best = &s;
            distance = d;
        }
    }
    return best->Source;
}
Position EditGeometry::Move(Position p, int h, int v, float &preferred) const
{
    auto at = Locate(p);
    size_t index = 0;
    for (size_t i = 0; i < m_Stops.size(); ++i)
        if (m_Stops[i].Source.Scalar == at.Source.Scalar && m_Stops[i].Source.Line == at.Source.Line &&
            m_Stops[i].Point.x == at.Point.x)
        {
            index = i;
            break;
        }
    if (h)
        return m_Stops[size_t(std::clamp(int(index) + h, 0, int(m_Stops.size() - 1)))].Source;
    if (preferred < 0)
        preferred = at.Point.x;
    int line = int(at.Source.Line) + v;
    if (line < 0)
        return Edge(p, false);
    float best = INFINITY;
    Position result = p;
    for (auto &s : m_Stops)
        if (int(s.Source.Line) == line)
        {
            float d = std::abs(s.Point.x - preferred);
            if (d < best)
            {
                best = d;
                result = s.Source;
            }
        }
    return result;
}
Position EditGeometry::Edge(Position p, bool end) const
{
    auto at = Locate(p);
    auto result = at.Source;
    for (auto &s : m_Stops)
        if (s.Source.Line == at.Source.Line)
        {
            result = s.Source;
            if (!end)
                break;
        }
    return result;
}
std::vector<SelectionBox> EditGeometry::Selection(uint32_t begin, uint32_t end) const
{
    if (begin > end)
        std::swap(begin, end);
    std::vector<SelectionBox> boxes;
    for (auto &s : m_Segments)
        if (s.Begin >= begin && s.End <= end)
        {
            auto &ln = m_Layout->Lines[s.Line];
            boxes.push_back({{s.Left, ln.Baseline - m_Layout->Font->AscenderEm() * m_Layout->EmSize},
                             {s.Right - s.Left, m_Layout->Font->HeightEm() * m_Layout->EmSize}});
        }
    std::sort(boxes.begin(), boxes.end(), [](auto a, auto b) {
        return a.Position.y != b.Position.y ? a.Position.y < b.Position.y : a.Position.x < b.Position.x;
    });
    std::vector<SelectionBox> merged;
    for (auto b : boxes)
    {
        if (!merged.empty() && merged.back().Position.y == b.Position.y &&
            b.Position.x <= merged.back().Position.x + merged.back().Size.x + .0001f)
            merged.back().Size.x =
                std::max(merged.back().Size.x, b.Position.x + b.Size.x - merged.back().Position.x);
        else
            merged.push_back(b);
    }
    return merged;
}
} // namespace Hazel::Text
