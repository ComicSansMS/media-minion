#ifndef MEDIA_MINION_INCLUDE_GUARD_SERVER_CONFIGURATION_HPP_
#define MEDIA_MINION_INCLUDE_GUARD_SERVER_CONFIGURATION_HPP_

#include <cstdint>
#include <filesystem>
#include <optional>

namespace media_minion::server {

struct Configuration {
    std::uint16_t listening_port;
    enum class Protocol {
        ipv4,
        ipv6
    } protocol;
};

std::optional<Configuration> parseServerConfig(std::filesystem::path const& config_filepath);

}
#endif
