#include <media_minion/server/http_session.hpp>

#include <gbBase/Assert.hpp>
#include <gbBase/Log.hpp>
#include <gbBase/UnusedVariable.hpp>

#include <boost/beast/version.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/beast/websocket/rfc6455.hpp>

#include <string>
#include <string_view>
#include <type_traits>

namespace media_minion::server {

namespace {

template<typename T>
auto response_bad_request(boost::beast::http::request<T> const& request, std::string_view reason) {
    boost::beast::http::response<boost::beast::http::string_body> response{ boost::beast::http::status::bad_request,
                                                                            request.version() };
    response.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
    response.set(boost::beast::http::field::content_type, "text/html");
    response.keep_alive(request.keep_alive());
    response.body() = "Bad Request: " + std::string(reason);
    response.prepare_payload();
    return response;
}

template<typename T>
auto response_not_found(boost::beast::http::request<T> const& request, std::string_view target) {
    boost::beast::http::response<boost::beast::http::string_body> response{ boost::beast::http::status::not_found,
                                                                            request.version() };
    response.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
    response.set(boost::beast::http::field::content_type, "text/html");
    response.keep_alive(request.keep_alive());
    response.body() = std::string(target) + " not found.";
    response.prepare_payload();
    return response;
}
}

HttpSession::HttpSession(boost::asio::ip::tcp::socket&& session_socket)
    :m_socket(std::move(session_socket))
{
}

HttpSession::~HttpSession()
{}

void HttpSession::run()
{
    newRead();
}

void HttpSession::requestShutdown()
{
    m_socket.close();
}

void HttpSession::newRead()
{
    boost::beast::http::async_read(m_socket, m_buffer, m_request,
        [this](boost::system::error_code const& ec, std::size_t bytes_read)
        {
            onHttpRead(ec, bytes_read);
        });
}

void HttpSession::onHttpRead(boost::system::error_code const& ec, std::size_t bytes_read)
{
    GHULBUS_UNUSED_VARIABLE(bytes_read);
    if (ec) {
        if (ec == boost::beast::http::error::end_of_stream) {
            m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_send);
        } else if (ec == boost::asio::error::operation_aborted) {
            GHULBUS_LOG(Trace, "Session aborted in http read.");
        } else {
            GHULBUS_LOG(Error, "Error in http read: " << ec.message());
            if (onError) { onError(ec); }
        }
        return;
    }

    auto sendResponse = [this](auto&& response) {
        using response_t = std::decay_t<decltype(response)>;
        auto response_ptr = std::make_unique<response_t>(std::forward<decltype(response)>(response));
        auto& r = *response_ptr;
        boost::beast::http::async_write(m_socket, r,
            [this, response_ptr = std::move(response_ptr)](boost::system::error_code const& ec, std::size_t bytes) {
                onHttpWrite(ec, bytes, response_ptr->need_eof());
            });
    };

    if (boost::beast::websocket::is_upgrade(m_request)) {
        if (onWebsocketUpgrade) { onWebsocketUpgrade(std::move(m_socket), std::move(m_request)); }
        return;
    } else if ((m_request.method() != boost::beast::http::verb::get) &&
               (m_request.method() != boost::beast::http::verb::head)) {
        sendResponse(response_bad_request(m_request, m_request.method_string().to_string()));
    }

    sendResponse(response_not_found(m_request, m_request.target().to_string()));
}

void HttpSession::sendResponse(AnyResponse response)
{
    std::unique_ptr<AnyResponse> ar = std::make_unique<AnyResponse>(std::move(response));
    AnyResponse & resp = *ar;
    resp.async_write(m_socket,
        [this, response = std::move(ar)](boost::system::error_code const& ec, std::size_t bytes) {
            onHttpWrite(ec, bytes, response->need_eof());
        });
}

void HttpSession::onHttpWrite(boost::system::error_code const& ec, std::size_t bytes_written, bool close_requested)
{
    GHULBUS_UNUSED_VARIABLE(bytes_written);
    if (ec) {
        if (ec == boost::asio::error::operation_aborted) {
            GHULBUS_LOG(Trace, "Session aborted in http write.");
        } else {
            if (onError) { onError(ec); }
        }
        return;
    }

    if (close_requested) {
        m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_send);
        return;
    }

    m_request = {};
    newRead();
}

}
