#ifndef MEDIA_MINION_INCLUDE_GUARD_PLAYER_CONFIGURATION_HPP_
#define MEDIA_MINION_INCLUDE_GUARD_PLAYER_CONFIGURATION_HPP_

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace media_minion::player {

struct Configuration {
    std::string server_host;
    std::uint16_t server_port;
};

std::optional<Configuration> parsePlayerConfig(std::filesystem::path const& config_filepath);

}
#endif
