#ifndef MEDIA_MINION_INCLUDE_GUARD_SERVER_HTTP_LISTENER_HPP_
#define MEDIA_MINION_INCLUDE_GUARD_SERVER_HTTP_LISTENER_HPP_

#include <media_minion/server/callback_return.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <functional>

namespace media_minion::server {

    class HttpSession;

    class HttpListener {
    private:
        boost::asio::ip::tcp::acceptor m_acceptor;
        boost::asio::ip::tcp::socket m_accepting_socket;
    public:
        HttpListener(boost::asio::io_context& io_ctx);

        ~HttpListener();

        HttpListener(HttpListener const&) = delete;
        HttpListener& operator=(HttpListener const&) = delete;
        HttpListener(HttpListener&&) = delete;
        HttpListener& operator=(HttpListener&&) = delete;

        void run(boost::asio::ip::tcp protocol, std::uint16_t port);

        void requestShutdown();

        std::function<CallbackReturn(boost::system::error_code const&)> onError;
        std::function<void(boost::asio::ip::tcp::socket&&)> onNewConnection;

    private:
        void newAccept();
        void onAccept(boost::system::error_code const& ec);
    };

}
#endif
