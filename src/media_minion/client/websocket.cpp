#include <media_minion/client/websocket.hpp>

#include <media_minion/common/coroutine_support/awaitables.hpp>

#include <boost/asio/connect.hpp>
#include <boost/beast/http/message.hpp>

#include <gbBase/Assert.hpp>
#include <gbBase/Log.hpp>
#include <gbBase/UnusedVariable.hpp>

#include <string>

template<typename... Args>
struct std::experimental::coroutine_traits<media_minion::client::ExecutionToken, Args...> {
    using promise_type = media_minion::client::WebsocketPromise;
};

namespace media_minion::client {

ExecutionToken WebsocketPromise::get_return_object() {
    return ExecutionToken{ std::experimental::coroutine_handle<WebsocketPromise>::from_promise(*this) };
}

std::experimental::suspend_never WebsocketPromise::initial_suspend() {
    return {};
}

std::experimental::suspend_always WebsocketPromise::final_suspend() {
    return {};
}

void WebsocketPromise::return_value(boost::system::error_code const& ec)
{
    result = ec;
}

void WebsocketPromise::unhandled_exception()
{
    std::terminate();
}

ExecutionToken::ExecutionToken(std::experimental::coroutine_handle<WebsocketPromise> h)
    :m_coroutine(h)
{}

ExecutionToken::~ExecutionToken()
{
    m_coroutine.destroy();
}

boost::system::error_code ExecutionToken::run()
{
    m_coroutine.promise().io_ctx->run();
    return m_coroutine.promise().result;
}

Websocket::WebsocketSession::WebsocketSession(boost::asio::ip::tcp::socket&& s)
    :websocket(std::move(s))
{
}

Websocket::Websocket()
    :m_io_ctx(1), m_work(m_io_ctx.get_executor())
{
}

Websocket::~Websocket()
{
}

ExecutionToken Websocket::run(std::string_view host, std::string_view service)
{
    using namespace media_minion::coroutine;
    co_await IoContextProvider<WebsocketPromise>{ m_io_ctx };
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work = std::move(m_work);

    std::string hostname{ host };
    boost::asio::ip::tcp::resolver resolver(m_io_ctx);
    auto [ec1, results] = co_await AsyncResolve(resolver, host, service);
    if (ec1) { co_return ec1; }
    GHULBUS_LOG(Trace, "Resolved " << results.size() << " endpoints.");

    boost::asio::ip::tcp::socket socket(m_io_ctx);
    auto [ec2, endpoint] = co_await AsyncConnect(socket, results);
    if (ec2) { co_return ec2; }

    GHULBUS_LOG(Trace, "Successfully connected to " << endpoint);
    WebsocketSession ws_session(std::move(socket));

    char const* target = "/";
    auto [ec3] = co_await AsyncHandshake(ws_session.websocket, hostname, target);
    if (ec3) { co_return ec3; }
    GHULBUS_LOG(Info, "Websocket handshake success!");

    while (ws_session.websocket.is_open()) {
        auto [ec4, bytes_read] = co_await AsyncRead(ws_session.websocket, ws_session.buffer);
        if (ec4) { co_return ec4; }
    }

    co_return boost::system::error_code{};
}

}
