#ifndef MEDIA_MINION_INCLUDE_GUARD_PLAYER_UI_PLAYER_APPLICATION_HPP_
#define MEDIA_MINION_INCLUDE_GUARD_PLAYER_UI_PLAYER_APPLICATION_HPP_

#include <media_minion/player/configuration.hpp>

#include <QObject>

#include <memory>

namespace media_minion::player::ui {

class PlayerApplication : public QObject
{
    Q_OBJECT
private:
    struct Pimpl;
    std::unique_ptr<Pimpl> m_pimpl;
public:
    PlayerApplication(Configuration const& config);
    ~PlayerApplication();

    void run();
    void requestShutdown();
};

}
#endif
