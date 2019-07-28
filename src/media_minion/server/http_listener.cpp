#include <media_minion/server/http_listener.hpp>

#include <gbBase/Assert.hpp>
#include <gbBase/Log.hpp>

namespace media_minion::server {

HttpListener::HttpListener(boost::asio::io_context& io_ctx)
    :m_acceptor(io_ctx), m_accepting_socket(io_ctx)
{
}

HttpListener::~HttpListener()
{
}

void HttpListener::run(boost::asio::ip::tcp protocol, std::uint16_t port)
{
    boost::asio::ip::tcp::endpoint local_endp(protocol, port);

    m_acceptor.open(protocol);
    m_acceptor.bind(local_endp);
    m_acceptor.listen();
    GHULBUS_LOG(Info, "Http server listening on " << m_acceptor.local_endpoint().address() <<
                      ":" << m_acceptor.local_endpoint().port());

    newAccept();
}

void HttpListener::newAccept()
{
    m_acceptor.async_accept(m_accepting_socket,
        [this](boost::system::error_code const& ec) mutable {
        onAccept(ec);
    });
}

void HttpListener::requestShutdown()
{
    m_acceptor.close();
}

void HttpListener::onAccept(boost::system::error_code const& ec)
{
    if (ec) {
        if (ec == boost::asio::error::operation_aborted) {
            return;
        } else if (!onError || onError(ec) == CallbackReturn::Abort) {
            return;
        }
    } else {
        GHULBUS_LOG(Info, "New connection: " << m_accepting_socket.remote_endpoint().address() <<
                          ":" << m_accepting_socket.remote_endpoint().port());
        if (onNewConnection) {
            onNewConnection(std::move(m_accepting_socket));
        }
    }

    newAccept();
}

}
