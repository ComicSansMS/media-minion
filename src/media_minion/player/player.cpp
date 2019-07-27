#include <media_minion/player/ui/tray_icon.hpp>

#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication the_app(argc, argv);

    media_minion::player::ui::TrayIcon tray_icon;

    return the_app.exec();
}
