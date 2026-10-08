#pragma once
#include <filesystem>
#include <string>
namespace Hazel
{
enum class SDKState { NotConfigured, Missing, Incompatible, Ready };
struct SDKSelection
{
    SDKState State = SDKState::NotConfigured;
    std::filesystem::path Root;
    std::string Source, Diagnostic;
    explicit operator bool() const { return State == SDKState::Ready; }
    const char *Status() const;
};
// Native, bounded discovery/validation. No ImGui, preferences writes or child processes.
// Ready means SDK files/pinned tooling match; canonical preflight checks host compilers.
class HazelSDK
{
  public:
    static SDKSelection Discover(const std::filesystem::path &configured = {});
    static SDKSelection Discover(const std::filesystem::path &configured,
                                 const std::filesystem::path &executable);
    static SDKSelection Validate(const std::filesystem::path &root,
                                 const std::string &source = "Explicit override");
    static const char *SetupInstructions();
};
} // namespace Hazel
