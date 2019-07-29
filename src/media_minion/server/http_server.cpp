#include <media_minion/server/http_server.hpp>

#include <media_minion/server/http_listener.hpp>
#include <media_minion/server/http_session.hpp>
#include <media_minion/server/websocket_session.hpp>

#include <gbBase/Assert.hpp>
#include <gbBase/Log.hpp>

#include <boost/asio/defer.hpp>

#include <algorithm>

namespace media_minion::server {

HttpServer::HttpServer()
    :m_workGuard(m_io_ctx.get_executor()), m_listener(nullptr)
{
}

HttpServer::~HttpServer()
{
}

int HttpServer::run(boost::asio::ip::tcp protocol, std::uint16_t port)
{
    HttpListener listener{ m_io_ctx };
    m_listener = &listener;

    listener.onError = onError;
    listener.onNewConnection = [this](boost::asio::ip::tcp::socket&& s) {
        createHttpSession(std::move(s));
    };

    listener.run(protocol, port);

    bool is_done = false;
    while (!is_done) {
        try {
            m_io_ctx.run();
            is_done = true;
        } catch (boost::system::system_error& e) {
            if (!onError || onError(e.code()) == CallbackReturn::Abort) {
                break;
            }
        }
    }
    GHULBUS_LOG(Info, "Server shutting down.");

    return is_done ? 0 : 1;
}

void HttpServer::requestShutdown()
{
    boost::asio::post(m_io_ctx, [this]() {
        if (m_listener) { m_listener->requestShutdown(); }
        for (auto const& session : m_sessions) { session->requestShutdown(); }
        for (auto const& session : m_websocket_sessions) { session->requestShutdown(); }
        boost::asio::defer(m_io_ctx, [this]() {
                waitForShutdown(nullptr);
            });
    });
}

void HttpServer::waitForShutdown(std::shared_ptr<boost::asio::steady_timer> timer)
{
    if ((!m_sessions.empty()) || (!m_websocket_sessions.empty())) {
        if (!timer) {
            timer = std::make_shared<boost::asio::steady_timer>(m_io_ctx);
        }
        timer->expires_from_now(std::chrono::milliseconds(100));
        timer->async_wait([this, timer](boost::system::error_code const&) {
                waitForShutdown(std::move(timer));
            });
    } else {
        m_workGuard.reset();
    }
}

void HttpServer::createHttpSession(boost::asio::ip::tcp::socket&& s)
{
    HttpSession& session = *m_sessions.emplace_back(std::make_unique<HttpSession>(std::move(s)));
    session.onError = [this, ps = &session](boost::system::error_code const&) {
        requestRemoveSession(ps);
    };

    session.onWebsocketUpgrade = [this, ps = &session](boost::asio::ip::tcp::socket&& s,
                                                       boost::beast::http::request<boost::beast::http::string_body>&& r) {
        GHULBUS_LOG(Trace, "Websocket Upgrade requested.");
        createWebsocketSession(std::move(s), std::move(r));
        requestRemoveSession(ps);
    };

    session.run();
}

void HttpServer::createWebsocketSession(boost::asio::ip::tcp::socket&& s,
                                        boost::beast::http::request<boost::beast::http::string_body>&& r)
{
    WebsocketSession& session = *m_websocket_sessions.emplace_back(std::make_unique<WebsocketSession>(std::move(s)));
    session.onError = [this, ps = &session](boost::system::error_code const& ec) {
        GHULBUS_LOG(Error, "Error in websocket session: " << ec.message());
        requestRemoveSession(ps);
    };
    session.onOpen = [this, ps = &session]() {
        auto const& ep = ps->get_socket().remote_endpoint();
        GHULBUS_LOG(Info, "Websocket session successfully established with " << ep.address() <<
                          ":" << ep.port() << ".");
    };
    session.onClose = [this, ps = &session]() {
        GHULBUS_LOG(Trace, "Closed websocket session.");
        requestRemoveSession(ps);
    };
    session.onMessage = [this, ps = &session](std::string msg) {
        if (onWebsocketMessage) {
            onWebsocketMessage(std::move(msg));
        }
    };

    session.run(std::move(r));
}

void HttpServer::requestRemoveSession(HttpSession* s)
{
    boost::asio::post(m_io_ctx, [this, s]() {
        auto const it = std::find_if(begin(m_sessions), end(m_sessions), [s](auto const& p) { return p.get() == s; });
        if (it != end(m_sessions)) {
            m_sessions.erase(it);
        }
    });
}

void HttpServer::requestRemoveSession(WebsocketSession* s)
{
    boost::asio::post(m_io_ctx, [this, s]() {
        auto const it = std::find_if(begin(m_websocket_sessions), end(m_websocket_sessions),
            [s](auto const& p) { return p.get() == s; });
        if (it != end(m_websocket_sessions)) {
            m_websocket_sessions.erase(it);
        }
    });
}
}



