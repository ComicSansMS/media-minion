#ifndef MEDIA_MINION_INCLUDE_GUARD_SERVER_HTTP_SERVER_HPP_
#define MEDIA_MINION_INCLUDE_GUARD_SERVER_HTTP_SERVER_HPP_

#include <media_minion/server/callback_return.hpp>

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <functional>
#include <memory>
#include <vector>

namespace media_minion::server {

class HttpListener;
class HttpSession;

class HttpServer {
private:
    boost::asio::io_context m_io_ctx;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> m_workGuard;
    HttpListener* m_listener;
    std::vector<std::unique_ptr<HttpSession>> m_sessions;
public:
    HttpServer();

    ~HttpServer();

    [[nodiscard]] int run(boost::asio::ip::tcp protocol, std::uint16_t port);

    void requestShutdown();

    HttpServer(HttpServer const&) = delete;
    HttpServer& operator=(HttpServer const&) = delete;
    HttpServer(HttpServer&&) = delete;
    HttpServer& operator=(HttpServer&&) = delete;

    std::function<CallbackReturn(boost::system::error_code const&)> onError;
private:

};

}
#endif
