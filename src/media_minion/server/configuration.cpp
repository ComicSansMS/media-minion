#ifndef MEDIA_MINION_INCLUDE_GUARD_SERVER_APPLICATION_HPP_
#define MEDIA_MINION_INCLUDE_GUARD_SERVER_APPLICATION_HPP_

#include <media_minion/server/types.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <boost/thread/barrier.hpp>

#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace media_minion::server {

class Application {
private:
    std::vector<std::thread> m_threads;
    std::unique_ptr<boost::barrier> m_threadBarrier;

    boost::asio::io_context m_io_ctx;
    boost::asio::ip::tcp::acceptor m_acceptor;
    boost::asio::ip::tcp::socket m_acceptingSocket;
    std::vector<boost::asio::ip::tcp::socket> m_active_connections;
public:
    Application();

    ~Application();

    Application(Application const&) = delete;
    Application& operator=(Application const&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    int run();
private:
    void do_work();
};

}
#endif
