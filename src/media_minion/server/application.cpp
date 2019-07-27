#include <media_minion/server/application.hpp>

#include <gbBase/Assert.hpp>
#include <gbBase/Log.hpp>

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
    :m_config(config), m_workGuard(m_io_ctx.get_executor()),
     m_acceptor(m_io_ctx, get_protocol(m_config.protocol)), m_acceptingSocket(m_io_ctx)
{
}

Application::~Application()
{
}

int Application::run()
{
    GHULBUS_LOG(Info, "Starting Media Minion server...");

    boost::asio::ip::tcp::endpoint local_endp(get_protocol(m_config.protocol), m_config.listening_port);
    m_acceptor.bind(local_endp);
    m_acceptor.listen();
    m_acceptor.async_accept(m_acceptingSocket, [this](boost::system::error_code const& ec) { onAccept(ec); });

    GHULBUS_LOG(Info, "Listening on local port " << m_config.listening_port << ".");

    m_io_ctx.run();

    GHULBUS_LOG(Info, "Media Minion server shut down.");
    return 0;
}

void Application::requestShutdown()
{
    boost::asio::post(m_io_ctx.get_executor(), [this]() {
        m_acceptor.close();
        m_workGuard.reset();
    });
}

void Application::onAccept(boost::system::error_code const& ec)
{
    if (!ec) {
        GHULBUS_LOG(Info, "New connection: " << m_acceptingSocket.remote_endpoint().address() << ":" << m_acceptingSocket.remote_endpoint().port());
        m_active_connections.push_back(std::move(m_acceptingSocket));
        m_acceptor.async_accept(m_acceptingSocket, [this](boost::system::error_code const& ec) { onAccept(ec); });
    }
}

}
