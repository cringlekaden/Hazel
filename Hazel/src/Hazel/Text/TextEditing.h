#pragma once
#include "TextLayout.h"
#include <glm/glm.hpp>
#include <deque>
namespace Hazel::Text
{
enum class EditCommand : uint8_t
{
    Insert,
    Backspace,
    Delete,
    Left,
    Right,
    Up,
    Down,
    Home,
    End,
    All,
    Copy,
    Cut,
    Paste,
    Undo,
    Redo,
    Enter,
    Submit,
    Cancel,
    Next,
    Previous
};
struct EditInput
{
    EditCommand Command = EditCommand::Insert;
    std::string Text;
    bool Shift = false, Word = false;
};
struct EditLimits
{
    uint32_t Bytes = 4096, Graphemes = 1024, Lines = 64;
    bool Multiline = false, ReadOnly = false;
};
struct Position
{
    uint32_t Scalar = 0, Line = 0;
    bool Upstream = false;
};
struct EditSelection
{
    Position Anchor, Caret;
};
struct CaretStop
{
    Position Source;
    glm::vec2 Point{};
    float Height = 1;
};
struct SelectionBox
{
    glm::vec2 Position{}, Size{};
};
// CPU only: the same accepted shaped result supplies hit/caret/selection geometry.
class EditGeometry
{
  public:
    void Build(const Ref<const Layout> &layout);
    const std::vector<CaretStop> &Stops() const
    {
        return m_Stops;
    }
    Position Hit(glm::vec2 point) const;
    CaretStop Locate(Position position) const;
    Position Move(Position position, int horizontal, int vertical, float &preferred) const;
    Position Edge(Position position, bool end) const;
    std::vector<SelectionBox> Selection(uint32_t begin, uint32_t end) const;

  private:
    struct Segment
    {
        uint32_t Begin, End, Line;
        float Left, Right;
    };
    Ref<const Layout> m_Layout;
    std::vector<CaretStop> m_Stops;
    std::vector<Segment> m_Segments;
};
class Clipboard
{
  public:
    virtual ~Clipboard() = default;
    virtual bool Read(std::string &value, size_t limit, std::string &error) = 0;
    virtual bool Write(const std::string &value, std::string &error) = 0;
};
// Thread-confined value object: no scene, window, font cache or graphics owners.
class EditBuffer
{
  public:
    static constexpr uint32_t MaxBytes = 16384, MaxGraphemes = 4096, MaxLines = 256, HistoryCount = 32;
    static constexpr size_t HistoryBytes = 128 * 1024;
    EditBuffer()
    {
        Reset("", {});
    }
    explicit EditBuffer(const std::string &text, const EditLimits &limits)
    {
        Reset(text, limits);
    }
    void Reset(const std::string &text, const EditLimits &limits);
    bool Apply(const EditInput &input, const EditGeometry *geometry = nullptr,
               Clipboard *clipboard = nullptr);
    void Point(Position point, bool extend);
    void BreakUndo()
    {
        m_Coalescing = false;
    }
    const std::string &Value() const
    {
        return m_Text;
    }
    const EditSelection &Selection() const
    {
        return m_Selection;
    }
    std::string Selected() const;
    uint32_t Byte(uint32_t scalar) const;
    uint32_t UTF16(uint32_t scalar) const;
    uint32_t Graphemes() const;
    size_t UndoCount() const
    {
        return m_Undo.size();
    }
    size_t RetainedHistoryBytes() const;
    const EditLimits &Limits() const
    {
        return m_Limits;
    }
    static std::string Normalize(const std::string &value, const EditLimits &limits);

  private:
    struct State
    {
        std::string Value;
        EditSelection Selection;
    };
    void Analyze();
    void Restore(State state);
    void Record(EditCommand command);
    uint32_t Step(uint32_t at, int direction, bool word) const;
    void Select(Position position, bool extend);
    std::string m_Text;
    std::vector<uint32_t> m_Scalars, m_Bytes, m_UTF16;
    std::vector<bool> m_Grapheme, m_Word;
    EditLimits m_Limits;
    EditSelection m_Selection;
    std::deque<State> m_Undo, m_Redo;
    bool m_Coalescing = false;
    EditCommand m_Last = EditCommand::All;
    float m_Preferred = -1;
};
// Scalar encoding only; text always originates in committed character events.
std::string EncodeScalar(uint32_t scalar);
} // namespace Hazel::Text
