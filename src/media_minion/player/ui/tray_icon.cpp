#include <media_minion/player/ui/tray_icon.hpp>

namespace media_minion::player::ui {

TrayIcon::TrayIcon()
    :QSystemTrayIcon()
{
    QAction* quit_action = m_contextMenu.addAction("&Quit");
    connect(quit_action, &QAction::triggered, this, &TrayIcon::onQuitRequested);
    this->setContextMenu(&m_contextMenu);

    setIcon(QIcon(":/mm_player.ico"));
    setToolTip("Media Minion Player");
}

void TrayIcon::onQuitRequested()
{
    showMessage("MM Player", "Quit requested", QSystemTrayIcon::Information, 3500);
    emit requestShutdown();
}

}
