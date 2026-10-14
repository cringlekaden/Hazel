#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
namespace Hazel {
// Main-thread control of scene-owned voices; device callbacks never enter Hazel/Mono.
// WAV only in this bounded first implementation. Device failure returns false, logs once.
class AudioPlayback {
public:
    static void ValidateClip(const std::filesystem::path& file);
    AudioPlayback();
    ~AudioPlayback();
    AudioPlayback(const AudioPlayback&) = delete;
    AudioPlayback& operator=(const AudioPlayback&) = delete;
    bool Play(uint64_t owner, const std::filesystem::path& file, float gain, bool loop);
    void Stop(uint64_t owner);
private:
    struct State;
    std::unique_ptr<State> m_State;
};
}
