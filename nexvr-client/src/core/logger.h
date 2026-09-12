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
#include <vector>

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

    static void RotateIfOversized(const std::string& path) {
        WIN32_FILE_ATTRIBUTE_DATA attr{};
        if (GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &attr)) {
            LARGE_INTEGER size;
            size.HighPart = attr.nFileSizeHigh;
            size.LowPart = attr.nFileSizeLow;
            if (size.QuadPart > 10 * 1024 * 1024) { // > 10 MB
                std::string oldPath = path + ".old";
                MoveFileExA(path.c_str(), oldPath.c_str(), MOVEFILE_REPLACE_EXISTING);
            }
        }
    }

    void Init(const std::string& logPath) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_logFiles.empty()) return;

        RotateIfOversized(logPath);
        FILE* f = _fsopen(logPath.c_str(), "a", _SH_DENYNO);
        if (f) {
            std::fprintf(f, "=== VRInject Log Started: %s ===\n", Timestamp().c_str());
            std::fflush(f);
            m_logFiles.push_back(f);
            m_openedPaths.push_back(logPath);
        }
        OutputDebugStringA("[VRInject] Logger initialised\n");
    }

    void AddSecondaryPath(const std::string& path) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (path.empty()) return;
        for (const auto& p : m_openedPaths) {
            if (_stricmp(p.c_str(), path.c_str()) == 0) return;
        }

        RotateIfOversized(path);
        FILE* f = _fsopen(path.c_str(), "a", _SH_DENYNO);
        if (f) {
            std::fprintf(f, "=== VRInject Secondary Log Attached: %s ===\n", Timestamp().c_str());
            std::fflush(f);
            m_logFiles.push_back(f);
            m_openedPaths.push_back(path);
        }
    }

    void Shutdown() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (FILE* f : m_logFiles) {
            if (f) {
                std::fprintf(f, "=== VRInject Log Ended: %s ===\n", Timestamp().c_str());
                std::fclose(f);
            }
        }
        m_logFiles.clear();
        m_openedPaths.clear();
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
        for (FILE* f : m_logFiles) {
            if (f) {
                std::fprintf(f, "%s", lineBuf);
                std::fflush(f);
            }
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

    std::vector<FILE*> m_logFiles;
    std::vector<std::string> m_openedPaths;
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
