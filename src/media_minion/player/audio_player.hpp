#ifndef MEDIA_MINION_INCLUDE_GUARD_PLAYER_AUDIO_PLAYER_HPP_
#define MEDIA_MINION_INCLUDE_GUARD_PLAYER_AUDIO_PLAYER_HPP_

#include <gbAudio/AudioDevice.hpp>
#include <gbAudio/Buffer.hpp>
#include <gbAudio/QueuedSource.hpp>

#include <array>

namespace media_minion::player {

class AudioPlayer {
private:
    GhulbusAudio::AudioDevicePtr m_device;
    std::array<GhulbusAudio::BufferPtr, 16> m_buffers;
    GhulbusAudio::QueuedSourcePtr m_source;
public:
    AudioPlayer();

    AudioPlayer(AudioPlayer const&) = delete;
    AudioPlayer(AudioPlayer&&) = delete;
};

}
#endif
