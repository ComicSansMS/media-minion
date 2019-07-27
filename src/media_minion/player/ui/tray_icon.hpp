#ifndef MEDIA_MINION_INCLUDE_GUARD_PLAYER_UI_TRAY_ICON_HPP_
#define MEDIA_MINION_INCLUDE_GUARD_PLAYER_UI_TRAY_ICON_HPP_

#include <QSystemTrayIcon>

namespace media_minion::player::ui {

class TrayIcon : public QSystemTrayIcon
{
    Q_OBJECT
private:
public:
    TrayIcon();
};

}
#endif
