#include <ui/player_tray_icon.hpp>

#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication the_app(argc, argv);

    PlayerTrayIcon tray_icon;

    return the_app.exec();
}
