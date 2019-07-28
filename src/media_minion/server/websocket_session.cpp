#include <media_minion/server/websocket_session.hpp>

#include <boost/system/error_code.hpp>

namespace media_minion::server {

WebsocketSession::WebsocketSession(boost::asio::ip::tcp::socket&& session_socket)
    :m_websocket(std::move(session_socket))
{
}

WebsocketSession::~WebsocketSession()
{
}

void WebsocketSession::run(boost::beast::http::request<boost::beast::http::string_body>&& request)
{
    m_websocket.async_accept(request, [this](boost::system::error_code const& ec) { onAccept(ec); });
}

void WebsocketSession::requestShutdown()
{
    if (m_websocket.is_open()) {
        m_websocket.async_close(boost::beast::websocket::none,
                                [this](boost::system::error_code const& ec) { onCloseCompleted(ec); });
    } else {
        boost::asio::post(m_websocket.get_executor(), [this]() { onCloseCompleted({}); });
    }
}

boost::asio::ip::tcp::socket& WebsocketSession::get_socket()
{
    return m_websocket.next_layer();
}

void WebsocketSession::onAccept(boost::system::error_code const& ec)
{
    if (ec) {
        if (onError) { onError(ec); }
        return;
    }
    if (onOpen) {
        onOpen();
    }
}

void WebsocketSession::onCloseCompleted(boost::system::error_code const& ec)
{
    if (ec) {
        if (onError) { onError(ec); }
        return;
    }

    m_websocket.next_layer().close();
    if (onClose) {
        onClose();
    }
}


}
