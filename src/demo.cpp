#include <StreamIOContext.hpp>

#include <gbBase/Assert.hpp>
#include <gbBase/Log.hpp>
#include <gbBase/LogHandlers.hpp>

extern "C" {
#   include <libavcodec/avcodec.h>
#   include <libavformat/avformat.h>
}

#include <gsl_util.h>

#include <atomic>
#include <iostream>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

std::ostream& operator<<(std::ostream& os, AVMediaType const& media_type)
{
    switch(media_type)
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
    char buffer[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_make_error_string(buffer, sizeof(buffer), ec);
    return std::string(buffer);
}

void ffmpegLogHandler(void* avcl, int level, char const* fmt, va_list vl)
{
    int print_prefix = 1;
    auto const buffer_size = av_log_format_line2(avcl, level, fmt, vl, nullptr, 0, &print_prefix);
    std::vector<char> buffer(buffer_size + 1);
    av_log_format_line2(avcl, level, fmt, vl, buffer.data(), buffer_size, &print_prefix);
    Ghulbus::LogLevel log_level;
    switch(level) {
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
    if(buffer[buffer_size - 2] == '\n') { buffer[buffer_size - 2] = '\0' ; }
    GHULBUS_LOG_QUALIFIED(log_level, "[FFMPEG] " << buffer.data());
}

int main()
{
    Ghulbus::Log::initializeLogging();
    Ghulbus::Log::setLogLevel(Ghulbus::LogLevel::Info);
    Ghulbus::Log::Handlers::LogMultiSink downstream(Ghulbus::Log::Handlers::logToCout,
                                                    Ghulbus::Log::Handlers::logToWindowsDebugger);
    Ghulbus::Log::Handlers::LogAsync top_level(downstream);
    top_level.start();
    Ghulbus::Log::setLogHandler(top_level);

    av_register_all();
    avformat_network_init();
    av_log_set_callback(ffmpegLogHandler);
    std::unique_ptr<AVFormatContext, void(*)(AVFormatContext*)> format_context_storage(avformat_alloc_context(),
                                                                                       &avformat_free_context);
    auto format_context = format_context_storage.get();

    std::string filename("D:/Media/Warpaint/The Fool (2010)/Warpaint - 01 Set Your Arms Down.mp3");
    std::ifstream fin(filename, std::ios_base::binary);
    if(!fin) {
        exit(1);
    }

    std::size_t const IO_BUFFER_SIZE = 4096;
    StreamIOContext io_context(fin, IO_BUFFER_SIZE);

    format_context->pb = io_context.getContext();

    if(avformat_open_input(&format_context, filename.c_str(), nullptr, nullptr) != 0) {
        GHULBUS_LOG(Error, "Error opening file '" << filename << "'.");
    }

    if(avformat_find_stream_info(format_context, nullptr) < 0) {
        GHULBUS_LOG(Error, "Error reading stream info.");
    }

    int play = -1;
    for(unsigned int i=0; i<format_context->nb_streams; ++i) {
        AVStream* it = format_context->streams[i];
        GHULBUS_LOG(Info, "#" << i << " " << it->codecpar->codec_type);
        if(it->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            play = i;
        }
    }

    auto stream = format_context->streams[play];
    GHULBUS_LOG(Info, "Playing #" << play);
    int const length = static_cast<int>(stream->duration / stream->time_base.den) * stream->time_base.num;
    GHULBUS_LOG(Info, "Length: " << (length / 60) << ":" << ((length % 60 < 10) ? "0" : "") << (length % 60));

    auto codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if(codec == 0) {
        GHULBUS_LOG(Error, "Invalid codec.");
    }

    auto codec_context = std::unique_ptr<AVCodecContext, void(*)(AVCodecContext*)>(avcodec_alloc_context3(codec),
        [](AVCodecContext* ctx) { avcodec_free_context(&ctx); });

    auto res = avcodec_open2(codec_context.get(), codec, nullptr);
    if(res != 0) {
        GHULBUS_LOG(Error, "Error opening codec: " << translateErrorCode(res));
    }
    auto const codec_context_guard = gsl::finally([&codec_context]() { avcodec_close(codec_context.get()); });

    auto frame_deleter = [](AVFrame* f) { av_frame_free(&f); };
    std::unique_ptr<AVFrame, void(*)(AVFrame*)> frame(av_frame_alloc(), frame_deleter);
    GHULBUS_ASSERT(frame);

    AVPacket packet;
    av_init_packet(&packet);
    /*
    std::vector<uint8_t> buffer;
    std::size_t const AUDIO_BUFFER_SIZE = 20480;        // 20 KiB
    static_assert(AUDIO_BUFFER_SIZE >= AV_INPUT_BUFFER_MIN_SIZE, "Buffer too small.");
    buffer.resize(AUDIO_BUFFER_SIZE + AV_INPUT_BUFFER_PADDING_SIZE);    // mind the padding!
    */
    packet.data = nullptr;
    packet.size = 0;
    while (av_read_frame(format_context, &packet) == 0) {
        if(packet.stream_index == play) {
            break;
        }
    }

    AVDictionaryEntry *t = NULL;
    while (t = av_dict_get(format_context->metadata, "", t, AV_DICT_IGNORE_SUFFIX)) {
        GHULBUS_LOG(Info, "  " << t->key << " - " << t->value);
    }

    std::vector<char> raw_data;
    while(av_read_frame(format_context, &packet) == 0) {
        auto const packet_guard =
            gsl::finally([packet]() mutable { av_packet_unref(&packet); });     // capture by-value here! packet will
                                                                                // get overwritten by decode below
        res = avcodec_send_packet(codec_context.get(), &packet);
        if(res != 0) {
            GHULBUS_LOG(Error, "Codec send packet error: " << res);
            break;
        }
        for(;;) {
            res = avcodec_receive_frame(codec_context.get(), frame.get());
            if(res != 0) {
                if(res == AVERROR(EAGAIN)) {
                    // everything we read before was decoded; read more stuff and call send_packet again
                    break;
                } else {
                    GHULBUS_LOG(Error, "Codec receive frame error: " << res);
                    break;
                }
            }
        }
    }

    // flush cached frames
    avcodec_send_packet(codec_context.get(), nullptr);
    for(;;) {
        res = avcodec_receive_frame(codec_context.get(), frame.get());
        if(res == AVERROR_EOF) {
            GHULBUS_LOG(Info, "Done decoding.");
            break;
        } else {
            GHULBUS_LOG(Error, "Codec receive frame error: " << res);
            break;
        }
    }


    top_level.stop();
    Ghulbus::Log::shutdownLogging();
}
