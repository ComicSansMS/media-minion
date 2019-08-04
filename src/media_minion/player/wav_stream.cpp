#include <media_minion/player/wav_stream.hpp>

namespace media_minion::player {

WavStream::WavStream()
    :m_fin("nearer.wav", std::ios_base::binary), m_loader()
{
    m_loader.openWav(m_fin);
}

std::optional<GhulbusAudio::DataVariant> WavStream::pull()
{
    std::size_t const time_ms = 100;
    std::size_t const sample_size = GhulbusAudio::formatSampleSize(m_loader.getFormat());
    std::size_t const buffer_size = m_loader.getSamplingFrequency() / 1000 * time_ms * sample_size;
    if (buffer_size > m_loader.getDataSize() - m_loader.getCurrentReadPosition()) {
        if (m_loader.getDataSize() != m_loader.getCurrentReadPosition()) {
            return m_loader.readAll();
        } else {
            return std::nullopt;
        }
    }
    return m_loader.readChunk(buffer_size);
}

}
