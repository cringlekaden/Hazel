#include "hzpch.h"
#include "AudioPlayback.h"
// Preserve the pinned single-header source untouched. Compile implementation once.
#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_ENCODING
#define MA_NO_FLAC
#define MA_NO_MP3
#define MA_NO_GENERATION
#include "../../../vendor/miniaudio/miniaudio.h"
#include <map>
#include <cmath>
namespace Hazel {
struct AudioPlayback::State {
    ma_engine Engine{};
    bool Ready=false;
    struct Voice { ma_sound Sound{}; bool Ready=false; ~Voice(){if(Ready)ma_sound_uninit(&Sound);} };
    std::map<uint64_t,std::unique_ptr<Voice>> Voices;
    State() { Ready=ma_engine_init(nullptr,&Engine)==MA_SUCCESS;if(!Ready)HZ_CORE_WARN("Audio device unavailable; runtime continues with visual feedback"); }
    ~State(){Voices.clear();if(Ready)ma_engine_uninit(&Engine);}
};
void AudioPlayback::ValidateClip(const std::filesystem::path& file) {
    if(file.extension()!=".wav" || !std::filesystem::is_regular_file(file))throw std::runtime_error("Audio requires an existing WAV clip");
    ma_decoder decoder{};
#ifdef HZ_PLATFORM_WINDOWS
    auto result=ma_decoder_init_file_w(file.c_str(),nullptr,&decoder);
#else
    auto result=ma_decoder_init_file(file.c_str(),nullptr,&decoder);
#endif
    if(result!=MA_SUCCESS)throw std::runtime_error("Cannot decode WAV audio clip: "+file.generic_u8string());
    ma_uint64 frames=0;result=ma_decoder_get_length_in_pcm_frames(&decoder,&frames);ma_decoder_uninit(&decoder);
    if(result!=MA_SUCCESS || !frames)throw std::runtime_error("Audio clip has no decodable PCM frames: "+file.generic_u8string());
}
AudioPlayback::AudioPlayback() : m_State(std::make_unique<State>()) {}
AudioPlayback::~AudioPlayback()=default;
void AudioPlayback::Stop(uint64_t owner){m_State->Voices.erase(owner);}
bool AudioPlayback::Play(uint64_t owner,const std::filesystem::path& file,float gain,bool loop) {
    if(!std::isfinite(gain) || gain<0 || gain>1 || file.extension()!=".wav")throw std::invalid_argument("Audio requires a WAV clip and gain between 0 and 1");
    if(!std::filesystem::is_regular_file(file))throw std::runtime_error("Missing audio clip: "+file.generic_u8string());
    if(!m_State->Ready)return false;
    Stop(owner);
    for(auto it=m_State->Voices.begin();it!=m_State->Voices.end();) {
        if(ma_sound_at_end(&it->second->Sound))it=m_State->Voices.erase(it);else ++it;
    }
    if(m_State->Voices.size()>=32){HZ_CORE_WARN("Audio voice limit (32) reached");return false;}
    auto voice=std::make_unique<State::Voice>();
#ifdef HZ_PLATFORM_WINDOWS
    auto result=ma_sound_init_from_file_w(&m_State->Engine,file.c_str(),MA_SOUND_FLAG_DECODE|MA_SOUND_FLAG_NO_SPATIALIZATION,nullptr,nullptr,&voice->Sound);
#else
    auto result=ma_sound_init_from_file(&m_State->Engine,file.c_str(),MA_SOUND_FLAG_DECODE|MA_SOUND_FLAG_NO_SPATIALIZATION,nullptr,nullptr,&voice->Sound);
#endif
    if(result!=MA_SUCCESS){ // Uninitialized ma_sound must not be uninitialized.
        throw std::runtime_error("Cannot decode WAV audio clip: "+file.generic_u8string());
    }
    voice->Ready=true;
    ma_sound_set_volume(&voice->Sound,gain);ma_sound_set_looping(&voice->Sound,loop?MA_TRUE:MA_FALSE);
    if(ma_sound_start(&voice->Sound)!=MA_SUCCESS)return false;
    m_State->Voices.emplace(owner,std::move(voice));return true;
}
}
