#include <StreamIOContext.hpp>

#include <gbBase/Assert.hpp>
#include <gbBase/Exception.hpp>
#include <gbBase/Finally.hpp>
#include <gbBase/Log.hpp>
#include <gbBase/LogHandlers.hpp>

#include <gbAudio/Audio.hpp>
#include <gbAudio/AudioDevice.hpp>
#include <gbAudio/Buffer.hpp>
#include <gbAudio/QueuedSource.hpp>
#include <gbAudio/Source.hpp>

extern "C" {
#   include <libavcodec/avcodec.h>
#   include <libavformat/avformat.h>
}

#include <algorithm>
#include <atomic>
#include <iostream>
#include <fstream>
#include <limits>
#include <memory>
#include <mutex>
#include <numeric>
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

GhulbusAudio::DataStereo16Bit decode_frame_s16p(AVCodecContext* codec_context, AVFrame* frame)
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
                break;
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

GhulbusAudio::DataStereo16Bit decode_frame_s32(AVCodecContext* codec_context, AVFrame* frame)
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
                break;
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

template<typename T>
T crop(T v, T min, T max)
{
    return std::max(std::min(v, max), min);
}

GhulbusAudio::DataStereo16Bit decode_frame_flp(AVCodecContext* codec_context, AVFrame* frame)
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
                break;
            }
        }
        GHULBUS_ASSERT(av_get_bytes_per_sample(codec_context->sample_fmt) == sizeof(float));
        std::size_t const idx_base = audio_data.getNumberOfSamples();
        audio_data.resize(audio_data.getNumberOfSamples() + frame->nb_samples);
        for (int sample_idx = 0; sample_idx < frame->nb_samples; ++sample_idx) {
            float f1, f2;
            std::memcpy(&f1, frame->extended_data[0] + sizeof(float) * sample_idx, sizeof(float));
            f1 = crop(f1, -1.f, 1.f);
            audio_data[idx_base + sample_idx].left =  static_cast<int16_t>(f1 * 32767.f);
            std::memcpy(&f2, frame->extended_data[1] + sizeof(float) * sample_idx, sizeof(float));
            f2 = crop(f2, -1.f, 1.f);
            audio_data[idx_base + sample_idx].right = static_cast<int16_t>(f2 * 32767.f);
        }
    }
    return audio_data;
}

GhulbusAudio::DataVariant decode_packet(AVFormatContext* format_context, AVPacket* packet, AVCodecContext* codec_context,
                                      AVFrame* frame)
{
    auto const packet_guard =
        Ghulbus::finally([packet]() mutable { if (packet) { av_packet_unref(packet); } }); // capture by-value here! packet will
                                                                                           // get overwritten by decode below
    int res = avcodec_send_packet(codec_context, packet);
    if (res != 0) {
        GHULBUS_LOG(Error, "Codec send packet error: " << res);
        GHULBUS_THROW(Ghulbus::Exceptions::IOError{}, "Send Packet error.");
    }

    switch (codec_context->sample_fmt)
    {
    case AV_SAMPLE_FMT_S16P: return decode_frame_s16p(codec_context, frame);
    case AV_SAMPLE_FMT_S32:  return decode_frame_s32(codec_context, frame);
    case AV_SAMPLE_FMT_FLTP: return decode_frame_flp(codec_context, frame);
    default:
        GHULBUS_LOG(Error, "Unrecognized sample format " << av_get_sample_fmt_name(codec_context->sample_fmt));
        GHULBUS_THROW(Ghulbus::Exceptions::IOError{}, "Unrecognized sample format.");
    }
}

int main()
{
    Ghulbus::Log::initializeLogging();
    auto guard_shutdown_logging = Ghulbus::finally([]() { Ghulbus::Log::shutdownLogging(); });
    Ghulbus::Log::setLogLevel(Ghulbus::LogLevel::Info);
    Ghulbus::Log::Handlers::LogMultiSink downstream(Ghulbus::Log::Handlers::logToCout,
                                                    Ghulbus::Log::Handlers::logToWindowsDebugger);
    Ghulbus::Log::Handlers::LogAsync top_level(downstream);
    top_level.start();
    auto guard_stop_async_logging = Ghulbus::finally([&top_level]() { top_level.stop();; });
    Ghulbus::Log::setLogHandler(top_level);

    auto guard_shutdown_audio = Ghulbus::finally([]() { GhulbusAudio::initializeAudio(); });

    GhulbusAudio::AudioDevicePtr audio_device = GhulbusAudio::AudioDevice::create();

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
    auto const codec_context_guard = Ghulbus::finally([&codec_context]() { avcodec_close(codec_context.get()); });

    auto frame_deleter = [](AVFrame* f) { av_frame_free(&f); };
    std::unique_ptr<AVFrame, void(*)(AVFrame*)> frame(av_frame_alloc(), frame_deleter);
    GHULBUS_ASSERT(frame);

    AVPacket packet;
    av_init_packet(&packet);

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

    std::vector<GhulbusAudio::BufferPtr> audio_buffers;
    std::generate_n(std::back_inserter(audio_buffers), 5, [&]() { return audio_device->createBuffer(); });

    std::vector<GhulbusAudio::DataVariant> audio_data;
    while (av_read_frame(format_context, &packet) == 0) {
        auto const data = decode_packet(format_context, &packet, codec_context.get(), frame.get());
        audio_data.push_back(std::move(data));
    }
    // flush decoder
    audio_data.push_back(decode_packet(format_context, nullptr, codec_context.get(), frame.get()));

    GhulbusAudio::BufferPtr audio_buffer = audio_device->createBuffer();
    GhulbusAudio::DataStereo16Bit audio_data_acc{ 44100 };
    while(av_read_frame(format_context, &packet) == 0) {
        auto const packet_guard =
            Ghulbus::finally([packet]() mutable { av_packet_unref(&packet); }); // capture by-value here! packet will
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
            switch (codec_context->sample_fmt) {
            case AV_SAMPLE_FMT_S16P:
            {
                GHULBUS_ASSERT(av_get_bytes_per_sample(codec_context->sample_fmt) == sizeof(int16_t));
                GHULBUS_ASSERT(frame->channels == 2);
                GhulbusAudio::DataStereo16Bit audio_data{ static_cast<uint32_t>(codec_context->sample_rate) };
                audio_data.resize(frame->nb_samples);
                for (int sample_idx = 0; sample_idx < frame->nb_samples; ++sample_idx) {
                    std::memcpy(&audio_data[sample_idx].left,
                        frame->extended_data[0] + sizeof(int16_t) * sample_idx, sizeof(int16_t));
                    std::memcpy(&audio_data[sample_idx].right,
                        frame->extended_data[1] + sizeof(int16_t) * sample_idx, sizeof(int16_t));
                }
                audio_data_acc.append(audio_data);
            } break;
            case AV_SAMPLE_FMT_S32:
            {
                GHULBUS_ASSERT(av_get_bytes_per_sample(codec_context->sample_fmt) == sizeof(int32_t));
                GHULBUS_ASSERT(frame->channels == 2);
                GhulbusAudio::DataStereo16Bit audio_data{ static_cast<uint32_t>(codec_context->sample_rate) };
            } break;
            default:
                GHULBUS_LOG(Error, "Unrecognized sample format " << av_get_sample_fmt_name(codec_context->sample_fmt));
                break;
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

    audio_data.erase(
        std::remove_if(begin(audio_data), end(audio_data),
            [](GhulbusAudio::DataVariant const& v) { return GhulbusAudio::DataOp::getNumberOfSamples(v) == 0; }),
        end(audio_data));
    GhulbusAudio::QueuedSourcePtr audio_source = audio_device->createQueuedSource();
    std::size_t data_idx = 0;
    std::size_t data_size = audio_data.size();
    for (auto& b : audio_buffers) {
        b->setData(audio_data[data_idx]);
        audio_source->enqueueBuffer(*b, [&data_idx, &audio_data, data_size](GhulbusAudio::Buffer& b) {
                if (data_idx == data_size) { return GhulbusAudio::QueuedSource::BufferAction::Drop; }
                b.setData(audio_data[data_idx]);
                ++data_idx;
                return GhulbusAudio::QueuedSource::BufferAction::Reinsert;
            });
        ++data_idx;
    }

    std::chrono::milliseconds const total_length =
        std::accumulate(begin(audio_data), end(audio_data), std::chrono::milliseconds(0),
            [](std::chrono::milliseconds acc, GhulbusAudio::DataVariant const& v) {
                return acc + GhulbusAudio::DataOp::getTotalLength(v);
            });
    GHULBUS_LOG(Info, "Playing... (" << total_length.count() << "ms)");
    audio_source->play();
    while (audio_source->getPlayState() == GhulbusAudio::Playable::State::Playing) {
        audio_source->pump();
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
}
