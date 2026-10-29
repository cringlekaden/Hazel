#pragma once
#include "Hazel/Text/TextEditing.h"
namespace Hazel
{
struct TextInputCapabilities
{
    bool Committed = true, Clipboard = true, Preedit = false, CompositionCaret = false,
         CandidatePosition = false;
    const char *Backend = "GLFW committed characters; native composition protocol unavailable";
};
// Main application thread only. GLFW may allocate while retrieving OS clipboard
// contents; Hazel bounds its own copy before decoding or accepting any edit.
class NativeTextClipboard final : public Text::Clipboard
{
  public:
    bool Read(std::string &value, size_t limit, std::string &error) override;
    bool Write(const std::string &value, std::string &error) override;
};
TextInputCapabilities GetTextInputCapabilities();
} // namespace Hazel
