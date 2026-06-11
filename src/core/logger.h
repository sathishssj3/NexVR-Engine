#pragma once
// ============================================================================
// vrinject::Logger – Lightweight, header-only, thread-safe logger
//
// Writes timestamped lines to both a log file and the Windows debug output
// (viewable in Visual Studio Output or DbgView). Designed to be usable from
// inside an injected DLL where we cannot rely on stdout.
// ============================================================================

#ifndef VRINJECT_LOGGER_H
#define VRINJECT_LOGGER_H

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <mutex>
#include <string>
#include <regex>
#include <filesystem>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace vrinject {
namespace Logger {

// ---- Internal state (using static locals to stay header-only in C++11) -----

inline FILE*& GetLogFile() {
    static FILE* s_logFile = nullptr;
    return s_logFile;
}

inline std::mutex& GetMutex() {
    static std::mutex s_mutex;
    return s_mutex;
}

enum class Level {
    Debug,
    Info,
    Warn,
    Error
};

// ---- Helpers ---------------------------------------------------------------

inline const char* LevelToString(Level level) {
    switch (level) {
        case Level::Debug: return "DEBUG";
        case Level::Info:  return "INFO ";
        case Level::Warn:  return "WARN ";
        case Level::Error: return "ERROR";
        default:           return "?????";
    }
}

/// Returns a timestamp string in "YYYY-MM-DD HH:MM:SS.mmm" format.
inline std::string Timestamp() {
    using namespace std::chrono;
    auto now       = system_clock::now();
    auto ms        = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    auto timer     = system_clock::to_time_t(now);
    std::tm bt     = *std::localtime(&timer);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d.%03d",
                  bt.tm_year + 1900, bt.tm_mon + 1, bt.tm_mday,
                  bt.tm_hour, bt.tm_min, bt.tm_sec, static_cast<int>(ms.count()));
    return buf;
}

// ---- Public API ------------------------------------------------------------

/// Open the log file. Call once at startup.
inline void Init(const std::string& logPath) {
    std::lock_guard<std::mutex> lock(GetMutex());
    if (GetLogFile()) return;

    GetLogFile() = _fsopen(logPath.c_str(), "a", _SH_DENYNO);
    if (GetLogFile()) {
        std::fprintf(GetLogFile(), "=== VRInject Log Started: %s ===\n", Timestamp().c_str());
        std::fflush(GetLogFile());
    }
    OutputDebugStringA("[VRInject] Logger initialised\n");
}

/// Flush and close the log file. Call once at shutdown.
inline void Shutdown() {
    std::lock_guard<std::mutex> lock(GetMutex());
    if (GetLogFile()) {
        std::fprintf(GetLogFile(), "=== VRInject Log Ended: %s ===\n", Timestamp().c_str());
        std::fclose(GetLogFile());
        GetLogFile() = nullptr;
    }
    OutputDebugStringA("[VRInject] Logger shut down\n");
}

/// Core logging function – thread-safe, supports printf-style formatting.
inline void Log(Level level, const char* file, int line, const char* fmt, ...) {
    char userBuf[1024];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(userBuf, sizeof(userBuf), fmt, args);
    va_end(args);

    std::string safeMessage = userBuf;
    std::regex pathRegex(R"([A-Za-z]:\\[^\s]+\\([^\s\\]+))");
    safeMessage = std::regex_replace(safeMessage, pathRegex, "$1");

    const char* filename = file;
    if (const char* slash = std::strrchr(file, '\\'))
        filename = slash + 1;
    else if (const char* fslash = std::strrchr(file, '/'))
        filename = fslash + 1;

    char lineBuf[2048];
    std::snprintf(lineBuf, sizeof(lineBuf),
                  "[%s] %s %s:%d | %s\n",
                  Timestamp().c_str(),
                  LevelToString(level),
                  filename,
                  line,
                  safeMessage.c_str());

    OutputDebugStringA(lineBuf);

    std::lock_guard<std::mutex> lock(GetMutex());
    if (GetLogFile()) {
        std::fprintf(GetLogFile(), "%s", lineBuf);
        std::fflush(GetLogFile());
    }
}

} // namespace Logger
} // namespace vrinject

// ---- Convenience macros ----------------------------------------------------
// Usage: LOG_INFO("Hooked Present at %p", addr);

#define LOG_DEBUG(fmt, ...) \
    vrinject::Logger::Log(vrinject::Logger::Level::Debug, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_INFO(fmt, ...) \
    vrinject::Logger::Log(vrinject::Logger::Level::Info, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_WARN(fmt, ...) \
    vrinject::Logger::Log(vrinject::Logger::Level::Warn, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_ERROR(fmt, ...) \
    vrinject::Logger::Log(vrinject::Logger::Level::Error, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#endif // VRINJECT_LOGGER_H
