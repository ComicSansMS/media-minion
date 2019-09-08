#include <media_minion/player/ffmpeg_stream.hpp>

#include <media_minion/common/result.hpp>

#include <gbBase/Log.hpp>
#include <gbBase/Finally.hpp>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4244)
#endif
extern "C" {
#   include <libavcodec/avcodec.h>
#   include <libavformat/avformat.h>
#   include <libavformat/avio.h>
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <algorithm>
#include <fstream>
#include <string>
#include <unordered_map>
#include <utility>

namespace {
void ffmpegLogHandler(void* avcl, int level, char const* fmt, va_list vl)
{
    int print_prefix = 1;
    int const buffer_size = av_log_format_line2(avcl, level, fmt, vl, nullptr, 0, &print_prefix);
    std::vector<char> buffer(static_cast<std::size_t>(buffer_size) + 1);
    av_log_format_line2(avcl, level, fmt, vl, buffer.data(), buffer_size, &print_prefix);
    Ghulbus::LogLevel log_level;
    switch (level) {
    default:
    case AV_LOG_PANIC:
    case AV_LOG_FATAL:
        log_level = Ghulbus::LogLevel::Critical;
        break;
    case AV_LOG_ERROR:
        log_level = Ghulbus::LogLevel::Error;
        break;
    case AV_LOG_WARNING:
        log_level = Ghulbus::LogLevel::Warning;
        break;
    case AV_LOG_INFO:
        log_level = Ghulbus::LogLevel::Info;
        break;
    case AV_LOG_VERBOSE:
    case AV_LOG_DEBUG:
        log_level = Ghulbus::LogLevel::Debug;
        break;
    case AV_LOG_TRACE:
        log_level = Ghulbus::LogLevel::Trace;
        break;
    }
    // remove trailing '\n' if any; GHULBUS_LOG will always linebreak after each message by itself
    if (buffer[buffer_size - 2] == '\n') { buffer[buffer_size - 2] = '\0'; }
    GHULBUS_LOG_QUALIFIED(log_level, "[FFMPEG] " << buffer.data());
}

std::string translateErrorCode(int ec)
{
    char buffer[AV_ERROR_MAX_STRING_SIZE] = { 0 };
    av_make_error_string(buffer, sizeof(buffer), ec);
    return std::string(buffer);
}

class StreamIOContext {
private:
    std::istream& m_fin;
    std::unique_ptr<unsigned char, void(*)(void*)> m_io_buffer;
    std::unique_ptr<AVIOContext, void(*)(AVIOContext*)> m_io_context_storage;
public:
    StreamIOContext(std::istream& stream, std::size_t buffer_size);
    AVIOContext* getContext() const;
private:
    static int static_io_read_packet(void* fstr, uint8_t* buf, int buf_size);
    static int64_t static_io_seek(void* fstr, int64_t offset, int whence);
    int io_read_packet(uint8_t* buf, int buf_size);
    int64_t io_seek(int64_t offset, int whence);
};

StreamIOContext::StreamIOContext(std::istream& stream, std::size_t buffer_size)
    :m_fin(stream),
    m_io_buffer(static_cast<unsigned char*>(av_malloc(buffer_size)), [](void*) { /* will be freed by the io_context */ }),
    m_io_context_storage(avio_alloc_context(m_io_buffer.get(), static_cast<int>(buffer_size), 0, this,
        StreamIOContext::static_io_read_packet, nullptr,
        StreamIOContext::static_io_seek),
        [](AVIOContext* ctx) { av_free(ctx->buffer); av_free(ctx); })
{
}

AVIOContext* StreamIOContext::getContext() const
{
    return m_io_context_storage.get();
}

int StreamIOContext::static_io_read_packet(void* fstr, uint8_t* buf, int buf_size)
{
    if (buf_size == 0) { return 0; }
    auto* self = static_cast<StreamIOContext*>(fstr);
    return self->io_read_packet(buf, buf_size);
};

int StreamIOContext::io_read_packet(uint8_t* buf, int buf_size)
{
    m_fin.read(reinterpret_cast<char*>(buf), buf_size);
    if (m_fin.fail() && !m_fin.eof()) {
        return -1;
    }
    int ret = static_cast<int>(m_fin.gcount());
    return ret;
}

int64_t StreamIOContext::static_io_seek(void* fstr, int64_t offset, int whence)
{
    auto* self = static_cast<StreamIOContext*>(fstr);
    return self->io_seek(offset, whence);
};

int64_t StreamIOContext::io_seek(int64_t offset, int whence)
{
    int64_t ret;
    switch (whence)
    {
    case SEEK_SET:
        m_fin.seekg(offset, std::ios_base::beg);
        ret = m_fin.tellg();
        break;
    case SEEK_CUR:
        m_fin.seekg(offset, std::ios_base::cur);
        ret = m_fin.tellg();
        break;
    case SEEK_END:
        m_fin.seekg(offset, std::ios_base::end);
        ret = m_fin.tellg();
        break;
    case AVSEEK_SIZE:
    {
        auto pos = m_fin.tellg();
        m_fin.seekg(0, std::ios_base::end);
        auto pos_end = m_fin.tellg();
        m_fin.seekg(0, std::ios_base::beg);
        auto fsize = pos_end - m_fin.tellg();
        m_fin.seekg(pos);
        ret = fsize;
    } break;
    default:
        GHULBUS_UNREACHABLE_MESSAGE("Invalid whence token in seek.");
    }
    if (m_fin.fail()) { return -1; }
    return ret;
}

media_minion::Result<GhulbusAudio::DataStereo16Bit> decode_frame_s16p(AVCodecContext* codec_context, AVFrame* frame)
{
    GHULBUS_PRECONDITION(codec_context->sample_rate > 0);
    GhulbusAudio::DataStereo16Bit audio_data{ static_cast<uint32_t>(codec_context->sample_rate) };
    for (;;) {
        int res = avcodec_receive_frame(codec_context, frame);
        if (res != 0) {
            if (res == AVERROR(EAGAIN)) {
                // everything we read before was decoded; read more stuff and call send_packet again
                break;
            } else if (res == AVERROR_EOF) {
                GHULBUS_LOG(Info, "Done decoding.");
                break;
            } else {
                GHULBUS_LOG(Error, "Codec receive frame error: " << res);
                return make_error_code(media_minion::errc::decode_error);
            }
        }
        GHULBUS_ASSERT(av_get_bytes_per_sample(codec_context->sample_fmt) == sizeof(int16_t));
        std::size_t const idx_base = audio_data.getNumberOfSamples();
        audio_data.resize(audio_data.getNumberOfSamples() + frame->nb_samples);
        for (int sample_idx = 0; sample_idx < frame->nb_samples; ++sample_idx) {
            std::memcpy(&audio_data[idx_base+ sample_idx].left,
                frame->extended_data[0] + sizeof(int16_t) * sample_idx, sizeof(int16_t));
            std::memcpy(&audio_data[idx_base+ sample_idx].right,
                frame->extended_data[1] + sizeof(int16_t) * sample_idx, sizeof(int16_t));
        }
    }
    return audio_data;
}

media_minion::Result<GhulbusAudio::DataStereo16Bit> decode_frame_s32(AVCodecContext* codec_context, AVFrame* frame)
{
    GHULBUS_PRECONDITION(codec_context->sample_rate > 0);
    GhulbusAudio::DataStereo16Bit audio_data{ static_cast<uint32_t>(codec_context->sample_rate) };
    for (;;) {
        int res = avcodec_receive_frame(codec_context, frame);
        if (res != 0) {
            if (res == AVERROR(EAGAIN)) {
                // everything we read before was decoded; read more stuff and call send_packet again
                break;
            } else if (res == AVERROR_EOF) {
                GHULBUS_LOG(Info, "Done decoding.");
                break;
            } else {
                GHULBUS_LOG(Error, "Codec receive frame error: " << res);
                return make_error_code(media_minion::errc::decode_error);
            }
        }
        GHULBUS_ASSERT(av_get_bytes_per_sample(codec_context->sample_fmt) == sizeof(int32_t));
        std::size_t const idx_base = audio_data.getNumberOfSamples();
        audio_data.resize(audio_data.getNumberOfSamples() + frame->nb_samples);
        for (int sample_idx = 0; sample_idx < frame->nb_samples; ++sample_idx) {
            int32_t s[2];
            std::memcpy(s, frame->extended_data[0] + sizeof(int32_t) * 2 * sample_idx, 2 * sizeof(int32_t));
            audio_data[sample_idx].left = s[0] >> 16;
            audio_data[sample_idx].right = s[1] >> 16;
        }
    }
    return audio_data;
}

media_minion::Result<GhulbusAudio::DataStereo16Bit> decode_frame_flp(AVCodecContext* codec_context, AVFrame* frame)
{
    GHULBUS_PRECONDITION(codec_context->sample_rate > 0);
    GhulbusAudio::DataStereo16Bit audio_data{ static_cast<uint32_t>(codec_context->sample_rate) };
    for (;;) {
        int res = avcodec_receive_frame(codec_context, frame);
        if (res != 0) {
            if (res == AVERROR(EAGAIN)) {
                // everything we read before was decoded; read more stuff and call send_packet again
                break;
            } else if (res == AVERROR_EOF) {
                GHULBUS_LOG(Info, "Done decoding.");
                break;
            } else {
                GHULBUS_LOG(Error, "Codec receive frame error: " << res);
                return make_error_code(media_minion::errc::decode_error);
            }
        }
        GHULBUS_ASSERT(av_get_bytes_per_sample(codec_context->sample_fmt) == sizeof(float));
        std::size_t const idx_base = audio_data.getNumberOfSamples();
        audio_data.resize(audio_data.getNumberOfSamples() + frame->nb_samples);
        for (int sample_idx = 0; sample_idx < frame->nb_samples; ++sample_idx) {
            float f1, f2;
            std::memcpy(&f1, frame->extended_data[0] + sizeof(float) * sample_idx, sizeof(float));
            f1 = std::clamp(f1, -1.f, 1.f);
            audio_data[idx_base + sample_idx].left =  static_cast<int16_t>(f1 * 32767.f);
            std::memcpy(&f2, frame->extended_data[1] + sizeof(float) * sample_idx, sizeof(float));
            f2 = std::clamp(f2, -1.f, 1.f);
            audio_data[idx_base + sample_idx].right = static_cast<int16_t>(f2 * 32767.f);
        }
    }
    return audio_data;
}

media_minion::Result<GhulbusAudio::DataVariant> decode_packet(AVFormatContext* format_context, AVPacket* packet,
                                                              AVCodecContext* codec_context, AVFrame* frame)
{
    auto const packet_guard =
        Ghulbus::finally([packet]() mutable { if (packet) { av_packet_unref(packet); } }); // capture by-value here! packet will
                                                                                           // get overwritten by decode below
    int res = avcodec_send_packet(codec_context, packet);
    if (res != 0) {
        GHULBUS_LOG(Error, "Codec send packet error: " << res);
        return make_error_code(media_minion::errc::decode_error);
    }

    switch (codec_context->sample_fmt)
    {
    case AV_SAMPLE_FMT_S16P: MM_OUTCOME_PROPAGATE(decode_frame_s16p(codec_context, frame));
    case AV_SAMPLE_FMT_S32:  MM_OUTCOME_PROPAGATE(decode_frame_s32(codec_context, frame));
    case AV_SAMPLE_FMT_FLTP: MM_OUTCOME_PROPAGATE(decode_frame_flp(codec_context, frame));
    default:
        GHULBUS_LOG(Error, "Unrecognized sample format " << av_get_sample_fmt_name(codec_context->sample_fmt));
        return make_error_code(media_minion::errc::decode_error);
    }
}
}

std::ostream& operator<<(std::ostream& os, AVMediaType const& media_type)
{
    switch (media_type)
    {
    case AVMEDIA_TYPE_UNKNOWN:    os << "Unknown type"; break;
    case AVMEDIA_TYPE_VIDEO:      os << "Video"; break;
    case AVMEDIA_TYPE_AUDIO:      os << "Audio"; break;
    case AVMEDIA_TYPE_DATA:       os << "Data"; break;
    case AVMEDIA_TYPE_SUBTITLE:   os << "Subtitle"; break;
    case AVMEDIA_TYPE_ATTACHMENT: os << "Attachment"; break;
    case AVMEDIA_TYPE_NB:         os << "NB"; break;
    default:
        GHULBUS_UNREACHABLE_MESSAGE("Invalid AVMEDIA_* enum.");
    }
    return os;
}

namespace media_minion::player {

struct FfmpegStream::Pimpl {
    std::unique_ptr<AVFormatContext, void(*)(AVFormatContext*)> m_formatContextStorage;
    AVFormatContext* m_formatContext;
    std::string m_filename;
    std::ifstream m_fin;
    StreamIOContext m_avIoContext;
    int m_avStreamIndex;
    std::unique_ptr<AVCodecContext, void(*)(AVCodecContext*)> m_avCodecContext;
    Ghulbus::AnyFinalizer m_guardCodecContextClose;
    std::unique_ptr<AVFrame, void(*)(AVFrame*)> m_avFrame;
    AVPacket m_avPacket;

    Pimpl();

    std::optional<int> findAudioStream();
    std::unordered_map<std::string, std::string> getTrackInfo();
    std::optional<GhulbusAudio::DataVariant> pull();
};

FfmpegStream::Pimpl::Pimpl()
    :m_formatContextStorage(avformat_alloc_context(), &avformat_free_context),
     m_formatContext(m_formatContextStorage.get()),
     m_filename("D:/Media/Warpaint/The Fool (2010)/Warpaint - 01 Set Your Arms Down.mp3"),
     m_fin(m_filename, std::ios_base::binary),
     m_avIoContext(m_fin, 8192), m_avStreamIndex(0),
     m_avCodecContext(nullptr, [](AVCodecContext* ctx) { avcodec_free_context(&ctx); }),
     m_avFrame(av_frame_alloc(), [](AVFrame* f) { av_frame_free(&f); })
{
    m_formatContext->pb = m_avIoContext.getContext();

    if (avformat_open_input(&m_formatContext, m_filename.c_str(), nullptr, nullptr) != 0) {
        GHULBUS_LOG(Error, "Error opening file '" << m_filename << "'.");
    }

    if (avformat_find_stream_info(m_formatContext, nullptr) < 0) {
        GHULBUS_LOG(Error, "Error reading stream info.");
    }

    auto const opt_stream_index = findAudioStream();
    if (!opt_stream_index) {
        GHULBUS_LOG(Error, "No audio stream found.");
    }
    m_avStreamIndex = *opt_stream_index;
    AVStream* avstream = m_formatContext->streams[m_avStreamIndex];

    auto const length = std::chrono::seconds((avstream->duration / avstream->time_base.den) *
                                             avstream->time_base.num);
    auto codec = avcodec_find_decoder(avstream->codecpar->codec_id);
    if (codec == 0) {
        GHULBUS_LOG(Error, "Invalid codec.");
    }

    m_avCodecContext.reset(avcodec_alloc_context3(codec));
    auto res = avcodec_open2(m_avCodecContext.get(), codec, nullptr);
    if (res != 0) {
        GHULBUS_LOG(Error, "Error opening codec: " << translateErrorCode(res));
    }
    m_guardCodecContextClose = Ghulbus::finally([this]() { avcodec_close(m_avCodecContext.get()); });

    av_init_packet(&m_avPacket);
    m_avPacket.data = nullptr;
    m_avPacket.size = 0;
    while (av_read_frame(m_formatContext, &m_avPacket) == 0) {
        if (m_avPacket.stream_index == m_avStreamIndex) {
            break;
        }
    }
    
}

std::optional<int> FfmpegStream::Pimpl::findAudioStream()
{
    GHULBUS_LOG(Trace, "Scanning a/v streams");
    std::optional<int> ret;
    if (m_formatContext->nb_streams > static_cast<unsigned int>(std::numeric_limits<int>::max())) { return ret; }
    for (int i = 0; i < static_cast<int>(m_formatContext->nb_streams); ++i) {
        AVStream* it = m_formatContext->streams[i];
        GHULBUS_LOG(Trace, " #" << i << " " << static_cast<AVMediaType>(it->codecpar->codec_type));
        if (it->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            ret = i;
        }
    }
    return ret;
}

std::unordered_map<std::string, std::string> FfmpegStream::Pimpl::getTrackInfo()
{
    std::unordered_map<std::string, std::string> ret;
    AVDictionaryEntry* t = NULL;
    while ((t = av_dict_get(m_formatContext->metadata, "", t, AV_DICT_IGNORE_SUFFIX)) != 0) {
        ret.emplace(std::make_pair(t->key, t->value));
    }
    return ret;
}

std::optional<GhulbusAudio::DataVariant> FfmpegStream::Pimpl::pull()
{
    auto const data = decode_packet(m_formatContext, &m_avPacket, m_avCodecContext.get(), m_avFrame.get());
    return data.has_value() ? std::make_optional(std::move(data).assume_value()) : std::nullopt;
}

void FfmpegStream::initializeFfmpeg()
{
    av_log_set_callback(ffmpegLogHandler);
}

FfmpegStream::FfmpegStream()
    :m_pimpl(std::make_unique<Pimpl>())
{
}

FfmpegStream::~FfmpegStream()
{
}

std::optional<GhulbusAudio::DataVariant> FfmpegStream::pull()
{
    return m_pimpl->pull();
}

}
