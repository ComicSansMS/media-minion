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

void AudioPlayer::play()
{
    for (auto& b : m_buffers) {
        auto const data = onDataRequest();
        if (!data) { break; }
        b->setData(*data);
        m_source->enqueueBuffer(*b, [this](GhulbusAudio::Buffer& b) -> GhulbusAudio::QueuedSource::BufferAction {
                auto const data = onDataRequest();
                if (data) {
                    b.setData(*data);
                    return GhulbusAudio::QueuedSource::BufferAction::Reinsert;
                }
                return GhulbusAudio::QueuedSource::BufferAction::Drop;
            });
    }

    m_source->play();
}

void AudioPlayer::pause()
{
    m_source->pause();
}

void AudioPlayer::stop()
{
    m_source->stop();
}

void AudioPlayer::clear()
{
    m_source->stop();
    m_source->clearQueue();
}

void AudioPlayer::pump()
{
    m_source->pump();
}

}
