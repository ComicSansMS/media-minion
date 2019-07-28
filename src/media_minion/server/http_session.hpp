#ifndef MEDIA_MINION_INCLUDE_GUARD_SERVER_HTTP_SESSION_HPP_
#define MEDIA_MINION_INCLUDE_GUARD_SERVER_HTTP_SESSION_HPP_

#include <media_minion/server/any_response.hpp>
#include <media_minion/server/any_request.hpp>

#include <boost/asio/ip/tcp.hpp>

#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>

#include <functional>

namespace media_minion::server {

class HttpSession {
private:
    boost::asio::ip::tcp::socket m_socket;
    boost::beast::flat_buffer m_buffer;
    boost::beast::http::request<boost::beast::http::string_body> m_request;
public:
    HttpSession(boost::asio::ip::tcp::socket&& session_socket);

    ~HttpSession();

    void run();

    HttpSession(HttpSession const&) = delete;
    HttpSession& operator=(HttpSession const&) = delete;
    HttpSession(HttpSession&&) = delete;
    HttpSession& operator=(HttpSession&&) = delete;

    void requestShutdown();

    std::function<void(boost::system::error_code const&)> onError;
    std::function<void(boost::asio::ip::tcp::socket&&,
                       boost::beast::http::request<boost::beast::http::string_body>&&)> onWebsocketUpgrade;
private:
    void newRead();
    void onHttpRead(boost::system::error_code const& ec, std::size_t bytes_read);
    void sendResponse(AnyResponse response);
    void onHttpWrite(boost::system::error_code const& ec, std::size_t bytes_written, bool close_requested);
};

}
#endif
