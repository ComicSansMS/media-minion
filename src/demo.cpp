#include <StreamIOContext.hpp>

extern "C" {
#   include <libavcodec/avcodec.h>
#   include <libavformat/avformat.h>
}

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
        // assert(false)
        break;
    }
    return os;
}

int main()
{
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
        std::cout << "Error opening file\n";
    }

    if(avformat_find_stream_info(format_context, nullptr) < 0)
    {
        std::cout << "Error reading stream info\n";
    }

    int play = -1;
    for(unsigned int i=0; i<format_context->nb_streams; ++i)
    {
        AVStream* it = format_context->streams[i];
        std::cout << "#" << i << " " << it->codec->codec_type << '\n';
        if(it->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
            play = i;
        }
    }

    auto stream = format_context->streams[play];
    std::cout << "\nPlaying #" << play << "\n\n";
    int const length = static_cast<int>(stream->duration / stream->time_base.den) * stream->time_base.num;
    std::cout << "Length: " << (length / 60) << ":" << ((length % 60 < 10) ? "0" : "") << (length % 60) << "\n";

    auto codec = avcodec_find_decoder(stream->codec->codec_id);
    if(codec == 0) {
        std::cout << "Invalid codec\n";
    }
}
