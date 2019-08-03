#include <media_minion/player/ui/tray_icon.hpp>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4251)
#endif
#include <QApplication>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

int main(int argc, char* argv[])
{
    QApplication the_app(argc, argv);

    media_minion::player::ui::TrayIcon tray_icon;

    return the_app.exec();
}
