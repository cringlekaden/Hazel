#pragma once
#include <imgui.h>
#include <string>

namespace Hazel::PropertyUI
{
// Presentation only. Owners validate/commit values, supply defaults and perform I/O.
struct EditResult
{
    bool Changed = false, Committed = false, ResetRequested = false;
    operator bool() const
    {
        return Changed;
    }
};
struct Options
{
    const char *Help = nullptr;
    const char *Validation = nullptr;
    const char *DisabledReason = nullptr;
};
class Row
{
  public:
    Row(const char *key, const char *label, Options options = {});
    ~Row();
    Row(const Row &) = delete;
    Row &operator=(const Row &) = delete;
    EditResult Result(bool changed, bool immediate = false) const;
    void Width(bool reset = false) const;
    bool Reset(bool available) const;

  private:
    Options m_Options;
    bool m_Table = false;
};
void Help(const char *text);
void Validation(const char *text);
void WrapButton(const char *label); // SameLine only when the next button fits.
EditResult DragFloat(const char *key, const char *label, float &value, float speed = .1f, float minimum = 0,
                     float maximum = 0, const char *format = "%.3f", const float *reset = nullptr,
                     Options options = {});
EditResult DragInt(const char *key, const char *label, int &value, float speed = 1, int minimum = 0,
                   int maximum = 0, const int *reset = nullptr, Options options = {});
EditResult SliderFloat(const char *key, const char *label, float &value, float minimum, float maximum,
                       const char *format = "%.2f", const float *reset = nullptr, Options options = {},
                       ImGuiSliderFlags flags = 0);
EditResult SliderInt(const char *key, const char *label, int &value, int minimum, int maximum,
                     const int *reset = nullptr, Options options = {});
EditResult Checkbox(const char *key, const char *label, bool &value, const bool *reset = nullptr,
                    Options options = {});
EditResult Combo(const char *key, const char *label, int &value, const char *items, Options options = {});
EditResult Text(const char *key, const char *label, std::string &draft, Options options = {},
                ImGuiInputTextFlags flags = 0);
EditResult Multiline(const char *key, const char *label, std::string &draft, Options options = {});
EditResult Scalar(const char *key, const char *label, ImGuiDataType type, void *value, Options options = {});
EditResult Double(const char *key, const char *label, double &value, double step = 0,
                  const char *format = "%.3f", Options options = {});
EditResult Vector(const char *key, const char *label, float *value, int axes, float speed = .1f,
                  const float *defaults = nullptr, const char *format = "%.2f", Options options = {},
                  float minimum = 0, float maximum = 0);
// Also used by axis buttons: reset is an intentional commit, even if already at default.
EditResult ResetAxis(float *value, int axes, int axis, const float *defaults);
EditResult SliderVector2(const char *key, const char *label, float *value, float minimum, float maximum,
                         Options options = {});
EditResult Color(const char *key, const char *label, float *value, Options options = {});
void ReadOnly(const char *key, const char *label, const char *value, Options options = {});
enum class ReferenceAction
{
    None,
    Browse,
    Clear,
    Open,
    Reveal
};
ReferenceAction Reference(const char *key, const char *label, const char *value, bool assigned,
                          Options options = {});
} // namespace Hazel::PropertyUI
