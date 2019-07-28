#ifndef MEDIA_MINION_INCLUDE_GUARD_SERVER_WEBSOCKET_SESSION_HPP_
#define MEDIA_MINION_INCLUDE_GUARD_SERVER_WEBSOCKET_SESSION_HPP_

#include <boost/asio/ip/tcp.hpp>

#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/websocket/stream.hpp>

#include <functional>

namespace media_minion::server {

class WebsocketSession {
private:
    boost::beast::websocket::stream<boost::asio::ip::tcp::socket> m_websocket;
    boost::beast::flat_buffer m_buffer;
public:
    WebsocketSession(boost::asio::ip::tcp::socket&& session_socket);

    ~WebsocketSession();

    void run(boost::beast::http::request<boost::beast::http::string_body>&& request);

    WebsocketSession(WebsocketSession const&) = delete;
    WebsocketSession& operator=(WebsocketSession const&) = delete;
    WebsocketSession(WebsocketSession&&) = delete;
    WebsocketSession& operator=(WebsocketSession&&) = delete;

    void requestShutdown();

    boost::asio::ip::tcp::socket& get_socket();

    std::function<void(boost::system::error_code const&)> onError;
    std::function<void()> onOpen;
    std::function<void()> onClose;
private:
    void onAccept(boost::system::error_code const& ec);
    void onCloseCompleted(boost::system::error_code const& ec);
    void newRead();
    void onWebsocketRead(boost::system::error_code const& ec, std::size_t bytes_read);
};

}
#endif
