#ifndef MEDIA_MINION_INCLUDE_GUARD_PLAYER_WAV_STREAM_HPP_
#define MEDIA_MINION_INCLUDE_GUARD_PLAYER_WAV_STREAM_HPP_

#include <gbAudio/Data.hpp>
#include <gbAudio/WavLoader.hpp>

#include <fstream>
#include <optional>

namespace media_minion::player {

class WavStream {
private:
    std::ifstream m_fin;
    GhulbusAudio::WavLoader m_loader;
public:
    WavStream();

    std::optional<GhulbusAudio::DataVariant> pull();
};

}
#endif
