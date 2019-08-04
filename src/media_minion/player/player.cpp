#include <media_minion/player/configuration.hpp>
#include <media_minion/player/ui/player_application.hpp>

#include <media_minion/common/logging.hpp>

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
    auto const guard_logging = media_minion::init_logging("mm_player.log");

    auto const opt_config = media_minion::player::parsePlayerConfig("player_config.json");
    if (!opt_config) {
        GHULBUS_LOG(Critical, "Invalid config file.");
        return 1;
    }
    media_minion::player::Configuration const& config = *opt_config;

    QApplication the_app(argc, argv);

    media_minion::player::ui::PlayerApplication player(config);
    player.run();

    return the_app.exec();
}
