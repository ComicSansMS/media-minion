#include <media_minion/player/ui/player_application.hpp>

#include <media_minion/player/ui/tray_icon.hpp>

#include <media_minion/player/audio_player.hpp>
#include <media_minion/player/ffmpeg_stream.hpp>
#include <media_minion/player/wav_stream.hpp>

#include <gbAudio/Audio.hpp>

#include <gbBase/Assert.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>

#include <QCoreApplication>

#include <chrono>
#include <thread>

namespace media_minion::player::ui {

struct PlayerApplication::Pimpl {
    Configuration m_config;

    boost::asio::io_context m_io_ctx;
    boost::asio::steady_timer m_audioTimer;

    AudioPlayer m_audio;
    WavStream m_wavStream;
    FfmpegStream m_ffmpegStream;

    std::thread m_thread;

    TrayIcon m_trayIcon;

    Pimpl(Configuration const& config);
    ~Pimpl();

    void run();
    void requestShutdown();

    void do_run();
private:
    void scheduleTimer();
};

PlayerApplication::Pimpl::Pimpl(Configuration const& config)
    :m_config(config), m_io_ctx(1), m_audioTimer(m_io_ctx)
{
    m_audio.onDataRequest = [this]() { return m_wavStream.pull(); };
}

PlayerApplication::Pimpl::~Pimpl()
{
    m_thread.join();
}

void PlayerApplication::Pimpl::run()
{
    GHULBUS_PRECONDITION(!m_thread.joinable());
    m_thread = std::thread([this]() { do_run(); });
}

void PlayerApplication::Pimpl::requestShutdown()
{
    m_audio.stop();
    m_io_ctx.stop();
}

void PlayerApplication::Pimpl::do_run()
{
    scheduleTimer();

    m_io_ctx.post([this]() { m_audio.play(); });

    m_io_ctx.run();
}

void PlayerApplication::Pimpl::scheduleTimer()
{
    m_audioTimer.expires_from_now(std::chrono::milliseconds(100));
    m_audioTimer.async_wait([this](boost::system::error_code const& ec) {
            if (ec) { return; }
            m_audio.pump();
            scheduleTimer();
        });
}


PlayerApplication::PlayerApplication(Configuration const& config)
    :m_pimpl(std::make_unique<Pimpl>(config))
{
    GhulbusAudio::initializeAudio();
    FfmpegStream::initializeFfmpeg();
    connect(&m_pimpl->m_trayIcon, &TrayIcon::requestShutdown, this, &PlayerApplication::requestShutdown);
}

PlayerApplication::~PlayerApplication()
{
    GhulbusAudio::shutdownAudio();
}

void PlayerApplication::run()
{
    m_pimpl->run();
}

void PlayerApplication::requestShutdown()
{
    m_pimpl->requestShutdown();
    QCoreApplication::quit();
}

}
