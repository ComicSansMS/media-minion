#include <media_minion/server/http_server.hpp>

#include <media_minion/server/http_listener.hpp>
#include <media_minion/server/http_session.hpp>

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
        HttpSession& session = *m_sessions.emplace_back(std::make_unique<HttpSession>(std::move(s)));
        session.onError = [this, ps = &session](boost::system::error_code const&) {
            auto const it = std::find_if(begin(m_sessions), end(m_sessions), [ps](auto const& p) { return p.get() == ps; });
            GHULBUS_ASSERT(it != end(m_sessions));
            m_sessions.erase(it);
        };
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
        m_workGuard.reset();
    });
}

}



