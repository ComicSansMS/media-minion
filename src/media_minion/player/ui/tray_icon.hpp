#ifndef MEDIA_MINION_INCLUDE_GUARD_PLAYER_UI_TRAY_ICON_HPP_
#define MEDIA_MINION_INCLUDE_GUARD_PLAYER_UI_TRAY_ICON_HPP_

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 5054)
#endif
#include <QSystemTrayIcon>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

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
