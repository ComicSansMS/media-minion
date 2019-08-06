#ifndef MEDIA_MINION_INCLUDE_GUARD_PLAYER_FFMPEG_STREAM_HPP_
#define MEDIA_MINION_INCLUDE_GUARD_PLAYER_FFMPEG_STREAM_HPP_

#include <gbAudio/Data.hpp>

#include <memory>
#include <optional>

namespace media_minion::player {

class FfmpegStream {
private:
    struct Pimpl;
    std::unique_ptr<Pimpl> m_pimpl;
public:
    static void initializeFfmpeg();

    FfmpegStream();
    ~FfmpegStream();

    std::optional<GhulbusAudio::DataVariant> pull();
};

}
#endif
