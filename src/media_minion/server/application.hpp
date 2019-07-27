#ifndef MEDIA_MINION_INCLUDE_GUARD_SERVER_APPLICATION_HPP_
#define MEDIA_MINION_INCLUDE_GUARD_SERVER_APPLICATION_HPP_

#include <media_minion/server/types.hpp>
#include <media_minion/server/configuration.hpp>

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace media_minion::server {

class Application {
private:
    Configuration m_config;

    boost::asio::io_context m_io_ctx;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> m_workGuard;
    boost::asio::ip::tcp::acceptor m_acceptor;
    boost::asio::ip::tcp::socket m_acceptingSocket;
    std::vector<boost::asio::ip::tcp::socket> m_active_connections;

public:
    Application(Configuration& config);

    ~Application();

    Application(Application const&) = delete;
    Application& operator=(Application const&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    int run();

    void requestShutdown();
private:
    void onAccept(boost::system::error_code const& ec);
};

}
#endif
