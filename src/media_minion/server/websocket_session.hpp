#ifndef MEDIA_MINION_INCLUDE_GUARD_SERVER_WEBSOCKET_SESSION_HPP_
#define MEDIA_MINION_INCLUDE_GUARD_SERVER_WEBSOCKET_SESSION_HPP_

#include <boost/asio/ip/tcp.hpp>

#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/websocket/stream.hpp>

#include <functional>

namespace media_minion::server {

class WebsocketSession {
private:
    boost::asio::ip::tcp::socket m_socket;
    boost::beast::flat_buffer m_buffer;
public:
    WebsocketSession(boost::asio::ip::tcp::socket&& session_socket);

    ~WebsocketSession();

    void run();

    WebsocketSession(WebsocketSession const&) = delete;
    WebsocketSession& operator=(WebsocketSession const&) = delete;
    WebsocketSession(WebsocketSession&&) = delete;
    WebsocketSession& operator=(WebsocketSession&&) = delete;

    void requestShutdown();

    std::function<void(boost::system::error_code const&)> onError;
private:
};

}
#endif
