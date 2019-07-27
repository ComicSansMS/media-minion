#include <media_minion/server/application.hpp>

#include <gbBase/Log.hpp>
#include <gbBase/LogHandlers.hpp>
#include <gbBase/Finally.hpp>

#include <iostream>
#include <thread>

struct LogFinalizers {
    Ghulbus::AnyFinalizer guard_log_shutdown;
    std::unique_ptr<Ghulbus::Log::Handlers::LogToFile> log_file;
#if defined WIN32
    std::unique_ptr<Ghulbus::Log::Handlers::LogMultiSink> log_multisink;
#endif
    std::unique_ptr<Ghulbus::Log::Handlers::LogAsync> log_async;
    Ghulbus::AnyFinalizer guard_log_async;
};

LogFinalizers init_logging()
{
    LogFinalizers ret;
    Ghulbus::Log::initializeLogging();
    ret.guard_log_shutdown = Ghulbus::finally([]() { Ghulbus::Log::shutdownLogging(); });

    Ghulbus::Log::setLogLevel(Ghulbus::LogLevel::Info);

    ret.log_file = std::make_unique<Ghulbus::Log::Handlers::LogToFile>("mm_server.log");
    Ghulbus::Log::LogHandler downstream = *ret.log_file;
#if defined WIN32
    ret.log_multisink = std::make_unique<Ghulbus::Log::Handlers::LogMultiSink>(*ret.log_file, Ghulbus::Log::Handlers::logToWindowsDebugger);
    downstream = *ret.log_multisink;
#endif

    ret.log_async = std::make_unique<Ghulbus::Log::Handlers::LogAsync>(downstream);
    Ghulbus::Log::setLogHandler(*ret.log_async);

    ret.log_async->start();
    ret.guard_log_async = Ghulbus::finally([lasync = ret.log_async.get()]{ lasync->stop(); });

    return ret;
}

int main()
{
    auto const guard_logging = init_logging();
    media_minion::server::Configuration config;
    config.protocol = media_minion::server::Configuration::Protocol::ipv4;
    config.listening_port = 13444;
    media_minion::server::Application server(config);

    int res = -1;
    std::thread t{ [&server, &res]() { res = server.run(); } };

    std::cout << "Type 'q' to quit." << std::endl;
    while (std::cin.get() != 'q') ;

    server.requestShutdown();
    t.join();

    return res;
}
