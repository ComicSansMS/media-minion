#ifndef MEDIA_MINION_INCLUDE_GUARD_COMMON_LOGGING_HPP_
#define MEDIA_MINION_INCLUDE_GUARD_COMMON_LOGGING_HPP_

#include <gbBase/LogHandlers.hpp>
#include <gbBase/Finally.hpp>

namespace media_minion {

struct LogFinalizers {
    Ghulbus::AnyFinalizer guard_log_shutdown;
    std::unique_ptr<Ghulbus::Log::Handlers::LogToFile> log_file;
#if defined WIN32
    std::unique_ptr<Ghulbus::Log::Handlers::LogMultiSink> log_multisink;
#endif
    std::unique_ptr<Ghulbus::Log::Handlers::LogAsync> log_async;
    Ghulbus::AnyFinalizer guard_log_async;
};

LogFinalizers init_logging(char const* logfile);

}

#endif
