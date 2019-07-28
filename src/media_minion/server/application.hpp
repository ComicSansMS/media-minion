#ifndef MEDIA_MINION_INCLUDE_GUARD_SERVER_APPLICATION_HPP_
#define MEDIA_MINION_INCLUDE_GUARD_SERVER_APPLICATION_HPP_

#include <media_minion/server/configuration.hpp>

#include <memory>
#include <vector>

namespace media_minion::server {

class HttpServer;

class Application {
private:
    Configuration m_config;

    std::unique_ptr<HttpServer> m_server;
public:
    Application(Configuration& config);

    ~Application();

    Application(Application const&) = delete;
    Application& operator=(Application const&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    int run();

    void requestShutdown();
};

}
#endif
