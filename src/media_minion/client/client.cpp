
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
    //ws.run("localhost", "13444");
    auto session = ws.run("localhost", "13444");
    GHULBUS_LOG(Trace, "Coroutine is up.");
    auto const ec = session.run();
    GHULBUS_LOG(Info, "Completed: " << ec.message());
}
