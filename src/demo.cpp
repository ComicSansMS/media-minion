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

int main()
{
    Ghulbus::Log::initializeLogging();
    Ghulbus::Log::setLogLevel(Ghulbus::LogLevel::Trace);
    Ghulbus::Log::Handlers::LogMultiSink downstream(Ghulbus::Log::Handlers::logToCout, Ghulbus::Log::Handlers::logToWindowsDebugger);
    Ghulbus::Log::Handlers::LogAsync top_level(downstream);
    top_level.start();
    Ghulbus::Log::setLogHandler(top_level);

    av_register_all();
    avformat_network_init();

    std::unique_ptr<AVFormatContext, void(*)(AVFormatContext*)> format_context_storage(avformat_alloc_context(), &avformat_free_context);
    auto format_context = format_context_storage.get();

    std::string filename("D:/Media/Warpaint/The Fool (2010)/Warpaint - 01 Set Your Arms Down.mp3");
    std::ifstream fin(filename, std::ios_base::binary);
    if(!fin) {
        exit(1);
    }

    std::size_t const IO_BUFFER_SIZE = 4096;
    StreamIOContext io_context(fin, IO_BUFFER_SIZE);

    format_context->pb = io_context.getContext();

    if(avformat_open_input(&format_context, filename.c_str(), nullptr, nullptr) != 0)
    {
        GHULBUS_LOG(Error, "Error opening file '" << filename << "'.");
    }

    if(avformat_find_stream_info(format_context, nullptr) < 0)
    {
        GHULBUS_LOG(Error, "Error reading stream info.");
    }

    int play = -1;
    for(unsigned int i=0; i<format_context->nb_streams; ++i)
    {
        AVStream* it = format_context->streams[i];
        GHULBUS_LOG(Info, "#" << i << " " << it->codec->codec_type);
        if(it->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
            play = i;
        }
    }

    auto stream = format_context->streams[play];
    GHULBUS_LOG(Info, "Playing #" << play);
    int const length = static_cast<int>(stream->duration / stream->time_base.den) * stream->time_base.num;
    GHULBUS_LOG(Info, "Length: " << (length / 60) << ":" << ((length % 60 < 10) ? "0" : "") << (length % 60));

    auto codec = avcodec_find_decoder(stream->codec->codec_id);
    if(codec == 0) {
        GHULBUS_LOG(Error, "Invalid codec.");
    }

    auto codec_context = std::unique_ptr<AVCodecContext, void(*)(AVCodecContext*)>(avcodec_alloc_context3(codec),
        [](AVCodecContext* ctx) { avcodec_free_context(&ctx); });

    auto res = avcodec_copy_context(codec_context.get(), format_context->streams[play]->codec);
    if(res != 0) {
        char buffer[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(res, buffer, sizeof(buffer));
        GHULBUS_LOG(Error, "Unable to copy codec context: " << buffer);
    }

    res = avcodec_open2(codec_context.get(), codec, nullptr);
    if(res != 0) {
        GHULBUS_LOG(Error, "Error opening codec: " << res);
    }
    gsl::finally([&codec_context]() { avcodec_close(codec_context.get()); });

    top_level.stop();
    Ghulbus::Log::shutdownLogging();
}
