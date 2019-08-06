#include <media_minion/player/ffmpeg_stream.hpp>

#include <gbBase/Log.hpp>

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

#include <fstream>
#include <string>

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
}

namespace media_minion::player {

struct FfmpegStream::Pimpl {
    std::unique_ptr<AVFormatContext, void(*)(AVFormatContext*)> m_formatContextStorage;
    AVFormatContext* m_formatContext;
    std::string m_filename;
    std::ifstream m_fin;
    StreamIOContext m_ioContext;

    Pimpl();
};

FfmpegStream::Pimpl::Pimpl()
    :m_formatContextStorage(avformat_alloc_context(), &avformat_free_context),
     m_formatContext(m_formatContextStorage.get()),
     m_filename("D:/Media/Warpaint/The Fool (2010)/Warpaint - 01 Set Your Arms Down.mp3"),
     m_fin(m_filename, std::ios_base::binary),
     m_ioContext(m_fin, 8192)
{
    m_formatContext->pb = m_ioContext.getContext();

    if (avformat_open_input(&m_formatContext, m_filename.c_str(), nullptr, nullptr) != 0) {
        GHULBUS_LOG(Error, "Error opening file '" << m_filename << "'.");
    }

    if (avformat_find_stream_info(m_formatContext, nullptr) < 0) {
        GHULBUS_LOG(Error, "Error reading stream info.");
    }

    int play = -1;
    for (unsigned int i = 0; i < m_formatContext->nb_streams; ++i) {
        AVStream* it = m_formatContext->streams[i];
        GHULBUS_LOG(Info, "#" << i << " " << it->codecpar->codec_type);
        if (it->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            play = i;
        }
    }
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
    return std::nullopt;
}

}
