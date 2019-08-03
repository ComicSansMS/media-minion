#include <media_minion/player/audio_player.hpp>

#include <gbAudio/Audio.hpp>

#include <gbBase/Log.hpp>

namespace media_minion::player {

AudioPlayer::AudioPlayer()
    :m_device(GhulbusAudio::AudioDevice::create()), m_source(m_device->createQueuedSource())
{
    for (auto& b : m_buffers) {
        b = m_device->createBuffer();
    }
}

}
