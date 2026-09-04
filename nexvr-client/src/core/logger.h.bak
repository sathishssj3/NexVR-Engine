#pragma once
// ============================================================================
// vrinject::Logger – Lightweight, header-only, thread-safe logger
//
/// @file logger.h
/// @brief Thread-safe logging utility for VRInject
///
/// This logger provides printf-style formatting with automatic timestamps,
/// thread safety via mutex, and output to both file and Windows debug output.
/// It's designed to be usable from within an injected DLL where we cannot
/// rely on standard output streams.
///
/// @details
/// Features:
/// - Thread-safe with std::mutex
/// - Timestamped log entries in [YYYY-MM-DD HH:MM:SS.mmm] format
/// - Configurable log levels (Debug, Info, Warn, Error)
/// - Automatic path stripping from file names in log output
/// - Output to both file and Visual Studio Debug output (via OutputDebugStringA)
///
/// @thread_safety
/// All public methods are thread-safe. Multiple threads can call Log() simultaneously.
///
/// @example
/// @code
/// LOG_INFO("Initialized module %s with version %d", moduleName, version);
/// LOG_WARN("Configuration file not found, using defaults");
/// LOG_ERROR("Failed to initialize DirectX device: 0x%08x", hr);
/// @endcode
///
/// @ingroup core
// ============================================================================

#ifndef VRINJECT_LOGGER_H
#define VRINJECT_LOGGER_H

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <mutex>
#include <string>
#include <regex>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace vrinject {
namespace Logger {

// ---- Internal state (using static locals to stay header-only in C++11) -----

/// @brief Get reference to the log file handle
/// @return Reference to static FILE pointer for the log file
inline FILE*& GetLogFile() {
    static FILE* s_logFile = nullptr;
    return s_logFile;
}

/// @brief Get reference to the mutex for thread safety
/// @return Reference to static mutex used for synchronizing log access
inline std::mutex& GetMutex() {
    static std::mutex s_mutex;
    return s_mutex;
}

/// @brief Log levels supported by the logger
enum class Level {
    Debug,   ///< Debug-level messages (typically disabled in release builds)
    Info,    ///< Informational messages about program flow
    Warn,    ///< Warning messages about potentially problematic situations
    Error    ///< Error messages indicating failures
};

// ---- Helpers ---------------------------------------------------------------

/// @brief Convert LogLevel enum to string representation
/// @param level The log level to convert
/// @return C-string representation of the level (e.g., "INFO ", "ERROR")
inline const char* LevelToString(Level level) {
    switch (level) {
        case Level::Debug: return "DEBUG";
        case Level::Info:  return "INFO ";
        case Level::Warn:  return "WARN ";
        case Level::Error: return "ERROR";
        default:           return "?????";
    }
}

/// @brief Get current timestamp formatted for logging
/// @return Timestamp string in "YYYY-MM-DD HH:MM:SS.mmm" format
/// @note Uses system clock and local time conversion
inline std::string Timestamp() {
    using namespace std::chrono;
    auto now       = system_clock::now();
    auto ms        = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    auto timer     = system_clock::to_time_t(now);
    std::tm bt{};
    localtime_s(&bt, &timer);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d.%03d",
                  bt.tm_year + 1900, bt.tm_mon + 1, bt.tm_mday,
                  bt.tm_hour, bt.tm_min, bt.tm_sec, static_cast<int>(ms.count()));
    return buf;
}

// ---- Public API ------------------------------------------------------------

/// @brief Initialize the logger with a log file path
/// @param logPath Full path to the log file to create/append to
/// @note Should be called once during DLL initialization
/// @thread_safety Thread-safe
/// @sa Shutdown()
inline void Init(const std::string& logPath) {
    std::lock_guard<std::mutex> lock(GetMutex());
    FILE* logFile = GetLogFile();
    if (logFile) return;  // Already initialized

    logFile = _fsopen(logPath.c_str(), "a", _SH_DENYNO);
    GetLogFile() = logFile;
    if (logFile) {
        std::fprintf(logFile, "=== VRInject Log Started: %s ===\n", Timestamp().c_str());
        std::fflush(logFile);
    }
    OutputDebugStringA("[VRInject] Logger initialised\n");
}

/// @brief Shutdown the logger and close the log file
/// @note Should be called once during DLL cleanup
/// @thread_safety Thread-safe
/// @sa Init()
inline void Shutdown() {
    std::lock_guard<std::mutex> lock(GetMutex());
    FILE* logFile = GetLogFile();
    if (logFile) {
        std::fprintf(logFile, "=== VRInject Log Ended: %s ===\n", Timestamp().c_str());
        std::fclose(logFile);
        GetLogFile() = nullptr;
    }
    OutputDebugStringA("[VRInject] Logger shut down\n");
}

/// @brief Core logging function - thread-safe with printf-style formatting
/// @param level The importance/severity level of the message
/// @param file  Source file name where the log call originated (typically __FILE__)
/// @param line  Line number in source file where the log call originated (typically __LINE__)
/// @param fmt   Format string (printf-style)
/// @param ...   Variable arguments matching the format specifiers in fmt
/// @note Automatically strips full paths from file parameter, leaving only the filename
/// @thread_safety Thread-safe - uses internal mutex to protect file access
///
/// @example
/// @code
/// Log(Level::Info, __FILE__, __LINE__, "Initialized %s successfully", moduleName);
/// @endcode
inline void Log(Level level, const char* file, int line, const char* fmt, ...) {
    char userBuf[1024];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(userBuf, sizeof(userBuf), fmt, args);
    va_end(args);

    std::string safeMessage = userBuf;

    // Safely remove paths (e.g. C:\... \filename.ext -> filename.ext)
    size_t pos = 0;
    while ((pos = safeMessage.find(":\\", pos)) != std::string::npos) {
        if (pos > 0 && isalpha(safeMessage[pos - 1])) {
            size_t pathStart = pos - 1;
            size_t pathEnd = pathStart;
            size_t lastSlash = pathStart;

            // Advance until we hit a character that usually terminates a path
            while (pathEnd < safeMessage.length()) {
                char c = safeMessage[pathEnd];
                if (c == ' ' || c == '\'' || c == '\"' || c == '\n' || c == '\r' || c == '\t') {
                    break;
                }
                if (c == '\\' || c == '/') {
                    lastSlash = pathEnd;
                }
                pathEnd++;
            }

            if (lastSlash > pathStart && lastSlash < pathEnd) {
                // Erase from start of path up to and including the last slash
                safeMessage.erase(pathStart, lastSlash - pathStart + 1);
                pos = pathStart; // Resume search from the new start
            } else {
                pos += 2;
            }
        } else {
            pos += 2;
        }
    }

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
    FILE* logFile = GetLogFile();
    if (logFile) {
        std::fprintf(logFile, "%s", lineBuf);
        std::fflush(logFile);
    }
}

} // namespace Logger
} // namespace vrinject

// ---- Convenience macros ----------------------------------------------------
// Usage: LOG_INFO("Hooked Present at %p", addr);

#ifndef NDEBUG
#define LOG_DEBUG(fmt, ...) \
    vrinject::Logger::Log(vrinject::Logger::Level::Debug, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define LOG_DEBUG(fmt, ...) do {} while(0)
#endif

#define LOG_INFO(fmt, ...) \
    vrinject::Logger::Log(vrinject::Logger::Level::Info, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_WARN(fmt, ...) \
    vrinject::Logger::Log(vrinject::Logger::Level::Warn, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_ERROR(fmt, ...) \
    vrinject::Logger::Log(vrinject::Logger::Level::Error, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#endif // VRINJECT_LOGGER_H