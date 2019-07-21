#ifndef MEDIA_MINION_PLAYER_INCLUDE_GUARD_UI_PLAYER_TRAY_ICON_HPP_
#define MEDIA_MINION_PLAYER_INCLUDE_GUARD_UI_PLAYER_TRAY_ICON_HPP_

#include <QSystemTrayIcon>

class PlayerTrayIcon : public QSystemTrayIcon
{
    Q_OBJECT
private:
public:
    PlayerTrayIcon();
};

#endif
