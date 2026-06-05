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

// ---- Internal state (defined inline so this stays header-only) -------------

inline std::ofstream g_logFile;
inline std::mutex    g_mutex;

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
    auto time_t_now = system_clock::to_time_t(now);

    std::tm local_tm{};
    localtime_s(&local_tm, &time_t_now);

    char buf[32];
    std::snprintf(buf, sizeof(buf),
                  "%04d-%02d-%02d %02d:%02d:%02d.%03lld",
                  local_tm.tm_year + 1900,
                  local_tm.tm_mon + 1,
                  local_tm.tm_mday,
                  local_tm.tm_hour,
                  local_tm.tm_min,
                  local_tm.tm_sec,
                  static_cast<long long>(ms.count()));
    return std::string(buf);
}

// ---- Public API ------------------------------------------------------------

/// Open the log file. Call once at startup.
inline void Init(const std::string& logPath) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_logFile.is_open()) return;           // already initialised

    // S6.2 Cap log file size and rotate
    std::error_code ec;
    if (std::filesystem::exists(logPath, ec)) {
        if (std::filesystem::file_size(logPath, ec) > 10 * 1024 * 1024) {
            std::filesystem::rename(logPath, logPath + ".old", ec);
        }
    }

    g_logFile.open(logPath, std::ios::out | std::ios::app); // append mode after rotation
    if (g_logFile.is_open()) {
        g_logFile << "=== VRInject Log Started: " << Timestamp() << " ===" << std::endl;
    }
    OutputDebugStringA("[VRInject] Logger initialised\n");
}

/// Flush and close the log file. Call once at shutdown.
inline void Shutdown() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_logFile.is_open()) {
        g_logFile << "=== VRInject Log Ended: " << Timestamp() << " ===" << std::endl;
        g_logFile.close();
    }
    OutputDebugStringA("[VRInject] Logger shut down\n");
}

/// Core logging function – thread-safe, supports printf-style formatting.
inline void Log(Level level, const char* file, int line, const char* fmt, ...) {
    // Format the user message first (on the stack to avoid allocations).
    char userBuf[1024];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(userBuf, sizeof(userBuf), fmt, args);
    va_end(args);

    // S6.1 Sanitize log string
    std::string safeMessage = userBuf;
    std::regex pathRegex(R"([A-Za-z]:\\[^\s]+\\([^\s\\]+))");
    safeMessage = std::regex_replace(safeMessage, pathRegex, "$1");

    // Strip path down to filename for readability.
    const char* filename = file;
    if (const char* slash = std::strrchr(file, '\\'))
        filename = slash + 1;
    else if (const char* fslash = std::strrchr(file, '/'))
        filename = fslash + 1;

    // Build the final line: "[TIMESTAMP] LEVEL filename:line | message"
    char lineBuf[2048];
    std::snprintf(lineBuf, sizeof(lineBuf),
                  "[%s] %s %s:%d | %s\n",
                  Timestamp().c_str(),
                  LevelToString(level),
                  filename,
                  line,
                  safeMessage.c_str());

    // Write to both destinations under the lock.
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_logFile.is_open()) {
            g_logFile << lineBuf;
            g_logFile.flush();               // flush every line; we're debugging
        }
    }

    // OutputDebugString is already thread-safe on Windows.
    OutputDebugStringA(lineBuf);
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
