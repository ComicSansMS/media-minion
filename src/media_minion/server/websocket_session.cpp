#include <media_minion/server/websocket_session.hpp>

#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/make_printable.hpp>
#include <boost/system/error_code.hpp>

#include <gbBase/Log.hpp>

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

    newRead();
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

void WebsocketSession::newRead()
{
    m_websocket.async_read(m_buffer, [this](boost::system::error_code const& ec, std::size_t bytes) {
            onWebsocketRead(ec, bytes);
        });
}

void WebsocketSession::onWebsocketRead(boost::system::error_code const& ec, std::size_t bytes_read)
{
    if (ec) {
        if (ec == boost::beast::websocket::error::closed) {
            if (onClose) { onClose(); }
        } else {
            if (onError) { onError(ec); }
        }
        return;
    }

    GHULBUS_LOG(Trace, "Received " << bytes_read << " from websocket: " <<
                       boost::beast::make_printable(m_buffer.data()));
    if (onMessage) {
        onMessage(boost::beast::buffers_to_string(m_buffer.data()));
    }
    m_buffer.consume(bytes_read);
    newRead();
}

}
