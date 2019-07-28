#include <media_minion/server/application.hpp>

#include <media_minion/server/http_server.hpp>

#include <gbBase/Assert.hpp>
#include <gbBase/Log.hpp>
#include <gbBase/UnusedVariable.hpp>

#include <algorithm>

namespace media_minion::server {
namespace {

boost::asio::ip::tcp get_protocol(Configuration::Protocol p)
{
    if (p == Configuration::Protocol::ipv4) {
        return boost::asio::ip::tcp::v4();
    } else {
        GHULBUS_ASSERT(p == Configuration::Protocol::ipv6);
        return boost::asio::ip::tcp::v6();
    }
}
}

Application::Application(Configuration& config)
    :m_config(config), m_server(std::make_unique<HttpServer>())
{
}

Application::~Application()
{
}

int Application::run()
{
    GHULBUS_LOG(Info, "Starting Media Minion server...");

    m_server->onError = [](boost::system::error_code const& ec) {
        GHULBUS_LOG(Error, "Error in Http server: " << ec.message());
        return CallbackReturn::Continue;
    };

    return m_server->run(get_protocol(m_config.protocol), m_config.listening_port);
}

void Application::requestShutdown()
{
    m_server->requestShutdown();
}

}
