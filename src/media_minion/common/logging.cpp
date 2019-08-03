#include <media_minion/common/logging.hpp>

namespace media_minion {

LogFinalizers init_logging(char const* logfile)
{
    LogFinalizers ret;
    Ghulbus::Log::initializeLogging();
    ret.guard_log_shutdown = Ghulbus::finally([]() { Ghulbus::Log::shutdownLogging(); });

    Ghulbus::Log::setLogLevel(Ghulbus::LogLevel::Trace);

    ret.log_file = std::make_unique<Ghulbus::Log::Handlers::LogToFile>(logfile);
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


}
