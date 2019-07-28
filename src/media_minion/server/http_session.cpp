#include <media_minion/server/http_session.hpp>

#include <gbBase/Assert.hpp>
#include <gbBase/Log.hpp>
#include <gbBase/UnusedVariable.hpp>

#include <boost/beast/http/read.hpp>
#include <boost/beast/websocket/rfc6455.hpp>

namespace media_minion::server {

HttpSession::HttpSession(boost::asio::ip::tcp::socket&& session_socket)
    :m_socket(std::move(session_socket))
{
}

HttpSession::~HttpSession()
{}

void HttpSession::run()
{
    boost::beast::http::async_read(m_socket, m_buffer, m_request,
        [this](boost::system::error_code const& ec, std::size_t bytes_read)
        {
            onHttpRead(ec, bytes_read);
        });
}

void HttpSession::requestShutdown()
{
    m_socket.close();
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
        }
        if (onError) { onError(ec); }
        return;
    }

    if (boost::beast::websocket::is_upgrade(m_request)) {
    } else if ((m_request.method() != boost::beast::http::verb::get) &&
               (m_request.method() != boost::beast::http::verb::head)) {
    }

}

}
