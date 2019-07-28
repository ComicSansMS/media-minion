#include <media_minion/server/http_server.hpp>

#include <media_minion/server/http_listener.hpp>
#include <media_minion/server/http_session.hpp>
#include <media_minion/server/websocket_session.hpp>

#include <gbBase/Assert.hpp>
#include <gbBase/Log.hpp>

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
    boost::asio::post(m_io_ctx.get_executor(), [this]() {
        if (m_listener) { m_listener->requestShutdown(); }
        for (auto const& session : m_sessions) { session->requestShutdown(); }
        for (auto const& session : m_websocket_sessions) { session->requestShutdown(); }
        m_workGuard.reset();
    });
}

void HttpServer::createHttpSession(boost::asio::ip::tcp::socket&& s)
{
    HttpSession& session = *m_sessions.emplace_back(std::make_unique<HttpSession>(std::move(s)));
    session.onError = [this, ps = &session](boost::system::error_code const&) {
        removeSession(ps);
    };

    session.onWebsocketUpgrade = [this, ps = &session](boost::asio::ip::tcp::socket&& s,
                                                       boost::beast::http::request<boost::beast::http::string_body>&& r) {
        GHULBUS_LOG(Trace, "Websocket Upgrade requested.");
        boost::asio::post(m_io_ctx, [this, ps, s = std::move(s), r = std::move(r)]() mutable {
                removeSession(ps);
                createWebsocketSession(std::move(s), std::move(r));
            });
    };

    session.run();
}

void HttpServer::createWebsocketSession(boost::asio::ip::tcp::socket&& s,
                                        boost::beast::http::request<boost::beast::http::string_body>&& r)
{
    WebsocketSession& session = *m_websocket_sessions.emplace_back(std::make_unique<WebsocketSession>(std::move(s)));
    session.onError = [this, ps = &session](boost::system::error_code const& ec) {
        GHULBUS_LOG(Error, "Error in websocket session: " << ec.message());
        removeSession(ps);
    };
    session.onOpen = [this, ps = &session]() {
        auto const& ep = ps->get_socket().remote_endpoint();
        GHULBUS_LOG(Info, "Websocket session successfully established with " << ep.address() <<
                          ":" << ep.port() << ".");
    };
    session.onClose = [this, ps = &session]() {
        GHULBUS_LOG(Trace, "Closed websocket session.");
        removeSession(ps);
    };

    session.run(std::move(r));
}

void HttpServer::removeSession(HttpSession* s)
{
    auto const it = std::find_if(begin(m_sessions), end(m_sessions), [s](auto const& p) { return p.get() == s; });
    GHULBUS_ASSERT(it != end(m_sessions));
    m_sessions.erase(it);
}

void HttpServer::removeSession(WebsocketSession* s)
{
    auto const it = std::find_if(begin(m_websocket_sessions), end(m_websocket_sessions),
                                 [s](auto const& p) { return p.get() == s; });
    GHULBUS_ASSERT(it != end(m_websocket_sessions));
    m_websocket_sessions.erase(it);
}
}



