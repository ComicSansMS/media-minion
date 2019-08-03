#include <media_minion/server/application.hpp>
#include <media_minion/server/configuration.hpp>

#include <media_minion/common/logging.hpp>

#include <gbBase/Log.hpp>

#include <iostream>
#include <thread>


int main()
{
    auto const guard_logging = media_minion::init_logging("mm_server.log");

    auto opt_config = media_minion::server::parseServerConfig("server_config.json");
    if (!opt_config) {
        GHULBUS_LOG(Critical, "Invalid server configuration.");
        return 1;
    }

    media_minion::server::Application server(*opt_config);

    int res = -1;
    std::thread t{ [&server, &res]() { res = server.run(); } };

    std::cout << "Type 'q' to quit." << std::endl;
    while (std::cin.get() != 'q') ;

    server.requestShutdown();
    t.join();

    return res;
}
