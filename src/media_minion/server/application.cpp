#include <media_minion/server/application.hpp>

#include <gbBase/Assert.hpp>
#include <gbBase/Log.hpp>
#include <gbBase/UnusedVariable.hpp>

#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/string_body.hpp>

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

struct Application::HttpSessionState {
    boost::asio::ip::tcp::socket* socket;
    std::unique_ptr<boost::beast::flat_buffer> buffer;
    std::unique_ptr<boost::beast::http::request<boost::beast::http::string_body>> request;
};

Application::Application(Configuration& config)
    :m_config(config), m_workGuard(m_io_ctx.get_executor()),
     m_acceptor(m_io_ctx, get_protocol(m_config.protocol))
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

    newAccept();

    GHULBUS_LOG(Info, "Listening on local port " << m_config.listening_port << ".");

    m_io_ctx.run();

    GHULBUS_LOG(Info, "Media Minion server shut down.");
    return 0;
}

void Application::newAccept()
{
    auto s = std::make_unique<boost::asio::ip::tcp::socket>(m_io_ctx);
    auto& accepting_socket = *s;
    m_acceptor.async_accept(accepting_socket,
        [this, s = std::move(s)](boost::system::error_code const& ec) mutable {
        onAccept(std::move(s), ec);
    });
}

void Application::requestShutdown()
{
    boost::asio::post(m_io_ctx.get_executor(), [this]() {
        m_acceptor.close();
        m_workGuard.reset();
    });
}

void Application::onAccept(std::unique_ptr<boost::asio::ip::tcp::socket>&& s, boost::system::error_code const& ec)
{
    if (!ec) {
        GHULBUS_LOG(Info, "New connection: " << s->remote_endpoint().address() << ":" << s->remote_endpoint().port());
        m_active_connections.push_back(std::move(s));
        HttpSessionState http_session;
        http_session.socket = m_active_connections.back().get();
        http_session.buffer = std::make_unique<boost::beast::flat_buffer>();
        http_session.request = std::make_unique<boost::beast::http::request<boost::beast::http::string_body>>();
        newHttpRead(std::move(http_session));
    }

    newAccept();
}

void Application::newHttpRead(HttpSessionState&& http_session)
{
    auto& socket = *http_session.socket;
    auto& buffer = *http_session.buffer;
    auto& request = *http_session.request;
    boost::beast::http::async_read(socket, buffer, request,
        [this, session = std::move(http_session)]
        (boost::system::error_code const& ec, std::size_t bytes_read) mutable
        {
            onHttpRead(std::move(session), ec, bytes_read);
        });
}

void Application::onHttpRead(HttpSessionState&& http_session,
                             boost::system::error_code const& ec, std::size_t bytes_read)
{

}

}
