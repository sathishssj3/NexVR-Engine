#pragma once

#include <string>
#include <memory>

namespace vrinject {

class ILogger {
public:
    enum class Level {
        Debug,
        Info,
        Warn,
        Error
    };

    virtual ~ILogger() = default;

    virtual void Init(const std::string& logPath) = 0;
    virtual void Shutdown() = 0;
    virtual void Log(Level level, const char* file, int line, const char* fmt, ...) = 0;
};

} // namespace vrinject
