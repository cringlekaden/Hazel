#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <glm/glm.hpp>

namespace Hazel
{
enum class FontStyle : uint8_t { Regular, Bold, Italic, BoldItalic };
namespace Text
{
// Canonical authored ranges are half-open decoded scalar ranges. A document-
// leading BOM is not a scalar. Conversion never rounds a byte/surrogate position.
enum class SourceUnit : uint8_t { Scalar, UTF8Byte, UTF16, Grapheme };
struct SourceBoundary
{
    uint32_t Byte=0, UTF16=0;
    bool Grapheme=false, Line=false, Mandatory=false;
};
struct SourceMap
{
    std::vector<uint32_t> Scalars;
    std::vector<SourceBoundary> Boundaries;
    static SourceMap Analyze(std::string_view source);
    uint32_t Scalar(uint32_t offset, SourceUnit unit, bool grapheme=true) const;
    uint32_t Offset(uint32_t scalar, SourceUnit unit) const;
    uint32_t Graphemes() const;
};
struct Span
{
    uint32_t Begin=0, End=0;
    std::optional<FontStyle> Face;
    std::optional<glm::vec4> Color;
    bool operator==(const Span& b) const {return Begin==b.Begin && End==b.End && Face==b.Face && Color==b.Color;}
};
constexpr uint32_t MaxSpans=128;
// Strict sorted/nonoverlapping input. Adjacent identical attributes coalesce;
// a range with no attributes is removed. All endpoints must be grapheme-safe.
std::vector<Span> NormalizeSpans(const SourceMap& source, const std::vector<Span>& spans);
std::vector<Span> NormalizeSpans(std::string_view source, const std::vector<Span>& spans);
void ValidateSpanFields(const std::vector<Span>& spans);
struct SourceEdit
{
    std::vector<Span> Spans;
    uint32_t Removed=0, Shifted=0;
};
// Preserve common grapheme prefix/suffix; remove intersected emphasis. One
// authored transaction owns this normalization and its source edit together.
SourceEdit RemapSpans(std::string_view before, std::string_view after, const std::vector<Span>& spans);
const Span* SpanAt(const std::vector<Span>& spans, uint32_t scalar);
}
}
