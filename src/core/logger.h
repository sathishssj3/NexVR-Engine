#pragma once

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

#include "core/i_logger.h"
#include "core/subsystem_context.h"

namespace vrinject {

class FileLogger : public ILogger {
public:
    FileLogger() = default;
    ~FileLogger() override { Shutdown(); }

    void Init(const std::string& logPath) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_logFile) return;

        m_logFile = _fsopen(logPath.c_str(), "a", _SH_DENYNO);
        if (m_logFile) {
            std::fprintf(m_logFile, "=== VRInject Log Started: %s ===\n", Timestamp().c_str());
            std::fflush(m_logFile);
        }
        OutputDebugStringA("[VRInject] Logger initialised\n");
    }

    void Shutdown() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_logFile) {
            std::fprintf(m_logFile, "=== VRInject Log Ended: %s ===\n", Timestamp().c_str());
            std::fclose(m_logFile);
            m_logFile = nullptr;
        }
        OutputDebugStringA("[VRInject] Logger shut down\n");
    }

    void Log(Level level, const char* file, int line, const char* fmt, ...) override {
        char userBuf[1024];
        va_list args;
        va_start(args, fmt);
        std::vsnprintf(userBuf, sizeof(userBuf), fmt, args);
        va_end(args);

        std::string safeMessage = userBuf;

        size_t pos = 0;
        while ((pos = safeMessage.find(":\\", pos)) != std::string::npos) {
            if (pos > 0 && isalpha(safeMessage[pos - 1])) {
                size_t pathStart = pos - 1;
                size_t pathEnd = pathStart;
                size_t lastSlash = pathStart;

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
                    safeMessage.erase(pathStart, lastSlash - pathStart + 1);
                    pos = pathStart;
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

        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_logFile) {
            std::fprintf(m_logFile, "%s", lineBuf);
            std::fflush(m_logFile);
        }
    }

private:
    const char* LevelToString(Level level) {
        switch (level) {
            case Level::Debug: return "DEBUG";
            case Level::Info:  return "INFO ";
            case Level::Warn:  return "WARN ";
            case Level::Error: return "ERROR";
            default:           return "?????";
        }
    }

    std::string Timestamp() {
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

    FILE* m_logFile = nullptr;
    std::mutex m_mutex;
};

} // namespace vrinject

#ifndef NDEBUG
#define LOG_DEBUG(fmt, ...) do { \
    if (auto logger = vrinject::SubsystemContext::Get().GetLogger()) { \
        logger->Log(vrinject::ILogger::Level::Debug, __FILE__, __LINE__, fmt, ##__VA_ARGS__); \
    } \
} while(0)
#else
#define LOG_DEBUG(fmt, ...) do {} while(0)
#endif

#define LOG_INFO(fmt, ...) do { \
    if (auto logger = vrinject::SubsystemContext::Get().GetLogger()) { \
        logger->Log(vrinject::ILogger::Level::Info, __FILE__, __LINE__, fmt, ##__VA_ARGS__); \
    } \
} while(0)

#define LOG_WARN(fmt, ...) do { \
    if (auto logger = vrinject::SubsystemContext::Get().GetLogger()) { \
        logger->Log(vrinject::ILogger::Level::Warn, __FILE__, __LINE__, fmt, ##__VA_ARGS__); \
    } \
} while(0)

#define LOG_ERROR(fmt, ...) do { \
    if (auto logger = vrinject::SubsystemContext::Get().GetLogger()) { \
        logger->Log(vrinject::ILogger::Level::Error, __FILE__, __LINE__, fmt, ##__VA_ARGS__); \
    } \
} while(0)

#endif // VRINJECT_LOGGER_H
