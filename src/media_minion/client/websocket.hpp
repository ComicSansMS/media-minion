#ifndef MEDIA_MINION_INCLUDE_GUARD_CLIENT_WEBSOCKET_HPP_
#define MEDIA_MINION_INCLUDE_GUARD_CLIENT_WEBSOCKET_HPP_

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/websocket/stream.hpp>

#include <functional>
#include <memory>
#include <string_view>

#include <experimental/coroutine>

namespace media_minion::client {

struct ExecutionToken;

struct WebsocketPromise {
    boost::asio::io_context* io_ctx;
    boost::system::error_code result;
    media_minion::client::ExecutionToken get_return_object();
    std::experimental::suspend_never initial_suspend();
    std::experimental::suspend_always final_suspend();
    void return_value(boost::system::error_code const& ec);
    void unhandled_exception();
};

struct ExecutionToken {
private:
    std::experimental::coroutine_handle<WebsocketPromise> m_coroutine;
public:
    ExecutionToken(std::experimental::coroutine_handle<WebsocketPromise> h);
    ~ExecutionToken();
    boost::system::error_code run();
};

class Websocket {
private:
    boost::asio::io_context m_io_ctx;
    std::string m_hostname;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> m_work;
    struct WebsocketSession {
        boost::beast::websocket::stream<boost::asio::ip::tcp::socket> websocket;
        boost::beast::flat_buffer buffer;

        WebsocketSession(boost::asio::ip::tcp::socket&& s);
    };
    std::unique_ptr<WebsocketSession> m_session;
public:
    Websocket();

    ~Websocket();

    ExecutionToken run(std::string_view host, std::string_view service);

    Websocket(Websocket const&) = delete;
    Websocket& operator=(Websocket const&) = delete;
    Websocket(Websocket&&) = delete;
    Websocket& operator=(Websocket&&) = delete;

    void requestShutdown();

    boost::asio::ip::tcp::socket& get_socket();

    std::function<void(boost::system::error_code const&)> onError;
    std::function<void()> onOpen;
    std::function<void()> onClose;
    std::function<void(std::string)> onMessage;
private:
    void onConnect(boost::asio::ip::tcp::socket&& s, std::string hostname);
    void onCloseCompleted(boost::system::error_code const& ec);
    void newRead();
    void onWebsocketRead(boost::system::error_code const& ec, std::size_t bytes_read);
};

}
#endif
