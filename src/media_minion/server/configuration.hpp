#ifndef MEDIA_MINION_INCLUDE_GUARD_SERVER_CONFIGURATION_HPP_
#define MEDIA_MINION_INCLUDE_GUARD_SERVER_CONFIGURATION_HPP_

#include <cstdint>

namespace media_minion::server {

struct Configuration {
    std::uint16_t listening_port;
    enum class Protocol {
        ipv4,
        ipv6
    } protocol;
};

}
#endif
