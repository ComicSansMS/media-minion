
#include <media_minion/client/websocket.hpp>

#include <boost/asio.hpp>

#include <gbBase/Finally.hpp>
#include <gbBase/Log.hpp>
#include <gbBase/LogHandlers.hpp>

int main()
{
    Ghulbus::Log::initializeLogging();
    auto guard_logShutdown = Ghulbus::finally([]() { Ghulbus::Log::shutdownLogging(); });
    Ghulbus::Log::setLogLevel(Ghulbus::LogLevel::Trace);

    media_minion::client::Websocket ws;
    ws.onError = [](boost::system::error_code const& ec) { GHULBUS_LOG(Error, ec.message()); };
    ws.run("localhost", "13444");
    boost::asio::io_context io_ctx;

    boost::asio::ip::tcp::resolver reslv(io_ctx);
    auto const resolved = reslv.resolve("localhost", "13444");
    GHULBUS_LOG(Info, "Found " << resolved.size() << " resolved endpoints.");

    boost::asio::ip::tcp::endpoint const endpoint = [&resolved]() {
        for (auto const& ep : resolved) {
            if (ep.endpoint().protocol() == boost::asio::ip::tcp::v4()) {
                GHULBUS_LOG(Info, "Choosing " << ep.host_name() << ":" << ep.service_name());
                return ep.endpoint();
            }
        }
        return boost::asio::ip::tcp::endpoint();
    }();

    boost::asio::ip::tcp::socket sock(io_ctx);
    sock.connect(endpoint);
    GHULBUS_LOG(Info, "Connected!");

    sock.close();
}
