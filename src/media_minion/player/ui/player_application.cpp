#include <media_minion/player/ui/player_application.hpp>

#include <media_minion/player/audio_player.hpp>

#include <gbAudio/Audio.hpp>

#include <gbBase/Assert.hpp>

#include <thread>

namespace media_minion::player::ui {

struct PlayerApplication::Pimpl {
    Configuration m_config;
    std::thread m_thread;
    AudioPlayer m_audio;

    Pimpl(Configuration const& config);

    void run();
    void requestShutdown();

    void do_run();
};

PlayerApplication::Pimpl::Pimpl(Configuration const& config)
    :m_config(config)
{
}

void PlayerApplication::Pimpl::run()
{
    GHULBUS_PRECONDITION(!m_thread.joinable());
    m_thread = std::thread([this]() { do_run(); });
}

void PlayerApplication::Pimpl::requestShutdown()
{
}

void PlayerApplication::Pimpl::do_run()
{

}


PlayerApplication::PlayerApplication(Configuration const& config)
    :m_pimpl(std::make_unique<Pimpl>(config))
{
    GhulbusAudio::initializeAudio();
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
}

}
