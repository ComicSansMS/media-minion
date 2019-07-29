#include <media_minion/client/websocket.hpp>

#include <boost/asio/connect.hpp>
#include <boost/beast/http/message.hpp>

#include <gbBase/Assert.hpp>
#include <gbBase/Log.hpp>
#include <gbBase/UnusedVariable.hpp>

#include <string>

namespace media_minion::client {

Websocket::WebsocketSession::WebsocketSession(boost::asio::ip::tcp::socket&& s)
    :websocket(std::move(s))
{
}

Websocket::Websocket()
    :m_work(m_io_ctx.get_executor())
{
}

Websocket::~Websocket()
{
}

void Websocket::run(std::string_view host, std::string_view service)
{
    auto resolver_ptr = std::make_unique<boost::asio::ip::tcp::resolver>(m_io_ctx);
    auto& resolver = *resolver_ptr;

    std::string hostname{ host };
    resolver.async_resolve(host, service,
        [this, resolver_ptr = std::move(resolver_ptr), hostname = std::move(hostname)]
        (boost::system::error_code const& ec, boost::asio::ip::tcp::resolver::results_type results) {
            if (ec) {
                if (onError) { onError(ec); }
                return;
            }
            GHULBUS_LOG(Trace, "Resolved " << results.size() << " endpoints.");
            auto endpoint_it = std::begin(results);
            auto socket_ptr = std::make_unique<boost::asio::ip::tcp::socket>(m_io_ctx);
            auto& socket = *socket_ptr;
            boost::asio::async_connect(socket, results,
                [this, socket_ptr = std::move(socket_ptr), hostname = std::move(hostname)]
                (boost::system::error_code const& ec, boost::asio::ip::tcp::endpoint const& endpoint) {
                    GHULBUS_UNUSED_VARIABLE(endpoint);
                    if (ec) {
                        if (onError) { onError(ec); }
                        return;
                    }
                    onConnect(std::move(*socket_ptr), std::move(hostname));
                });
        });

    m_io_ctx.run();
}

void Websocket::onConnect(boost::asio::ip::tcp::socket&& s, std::string hostname)
{
    m_session = std::make_unique<WebsocketSession>(std::move(s));
    auto& ws = m_session->websocket;
    ws.async_handshake(hostname, "/", [this](boost::system::error_code const& ec) {
            if (ec) {
                if (onError) { onError(ec); }
                return;
            }
            GHULBUS_LOG(Info, "Websocket handshake success!");
        });
    
}

}
