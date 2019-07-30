#include <media_minion/client/websocket.hpp>

#include <boost/asio/connect.hpp>
#include <boost/beast/http/message.hpp>

#include <gbBase/Assert.hpp>
#include <gbBase/Log.hpp>
#include <gbBase/UnusedVariable.hpp>

#include <string>

template<typename T>
struct ErrorResult {
    boost::system::error_code error;
    T result;
};

/// @todo CTAD
template<typename T>
ErrorResult<T> make_error_result(boost::system::error_code const& ec, T&& result) {
    return ErrorResult<T>{ ec, std::forward<T>(result) };
}

class AsyncResolve {
private:
    boost::asio::ip::tcp::resolver& m_resolver;
    std::string_view m_host;
    std::string_view m_service;
    boost::system::error_code m_ec;
    boost::asio::ip::tcp::resolver::results_type m_results;
public:
    AsyncResolve(boost::asio::ip::tcp::resolver& resolver, std::string_view host, std::string_view service)
        :m_resolver(resolver), m_host(host), m_service(service)
    {}

    bool await_ready() { return false; }

    void await_suspend(std::experimental::coroutine_handle<> h) {
        m_resolver.async_resolve(m_host, m_service,
            [this, h](boost::system::error_code const& ec, boost::asio::ip::tcp::resolver::results_type results) {
                m_ec = ec;
                m_results = std::move(results);
                h.resume();
            });
    }

    auto await_resume() {
        return make_error_result(m_ec, m_results);
    }
};

class AsyncConnect {
private:
    boost::asio::ip::tcp::socket& m_socket;
    boost::asio::ip::tcp::resolver::results_type const& m_resolverResults;
    boost::system::error_code m_ec;
    boost::asio::ip::tcp::endpoint m_endpoint;
public:
    AsyncConnect(boost::asio::ip::tcp::socket& socket, boost::asio::ip::tcp::resolver::results_type const& res_res)
        :m_socket(socket), m_resolverResults(res_res)
    {}

    bool await_ready() { return false; }

    void await_suspend(std::experimental::coroutine_handle<> h) {
        boost::asio::async_connect(m_socket, m_resolverResults,
            [this, h](boost::system::error_code const& ec, boost::asio::ip::tcp::endpoint const& endpoint) {
                m_ec = ec;
                m_endpoint = endpoint;
                h.resume();
            });
    }

    auto await_resume() {
        return make_error_result(m_ec, m_endpoint);
    }
};

class AsyncHandshake {
private:
    boost::beast::websocket::stream<boost::asio::ip::tcp::socket>& m_websocket;
    std::string& m_hostname;
    std::string& m_target;
    boost::system::error_code m_ec;
public:
    AsyncHandshake(boost::beast::websocket::stream<boost::asio::ip::tcp::socket>& websocket,
                   std::string& hostname, std::string& target)
        :m_websocket(websocket), m_hostname(hostname), m_target(target)
    {}

    bool await_ready() { return false; }

    void await_suspend(std::experimental::coroutine_handle<> h) {
        m_websocket.async_handshake(m_hostname, m_target, [this, h](boost::system::error_code const& ec) {
                m_ec = ec;
                h.resume();
            });
    }

    auto await_resume() {
        return m_ec;
    }
};

template<typename... Args>
struct std::experimental::coroutine_traits<media_minion::client::ExecutionToken, Args...> {
    using promise_type = media_minion::client::WebsocketPromise;
};

template<typename P>
struct IoContextProvider {
    boost::asio::io_context* m_io_ctx;
    IoContextProvider(boost::asio::io_context& io)
        :m_io_ctx(&io)
    {}

    bool await_ready() { return false; }

    bool await_suspend(std::experimental::coroutine_handle<P> h) {
        h.promise().io_ctx = m_io_ctx;
        return false;
    }

    void await_resume() {}
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
    :m_work(m_io_ctx.get_executor())
{
}

Websocket::~Websocket()
{
}

ExecutionToken Websocket::co_run(std::string_view host, std::string_view service)
{
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

    std::string target = "/";
    ec2 = co_await AsyncHandshake(ws_session.websocket, hostname, target);
    if (ec2) { co_return ec2; }

    GHULBUS_LOG(Info, "Websocket handshake success!");
    co_return boost::system::error_code{};
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
