#include <media_minion/server/websocket_session.hpp>

#include <boost/system/error_code.hpp>

namespace media_minion::server {

WebsocketSession::WebsocketSession(boost::asio::ip::tcp::socket&& session_socket)
    :m_socket(std::move(session_socket))
{
}

WebsocketSession::~WebsocketSession()
{
}

void WebsocketSession::run()
{
    if (onError) { onError(boost::system::errc::make_error_code(boost::system::errc::not_supported)); }
}

void WebsocketSession::requestShutdown()
{
    m_socket.close();
}


}
