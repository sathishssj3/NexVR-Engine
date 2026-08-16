#pragma once

#include <memory>
#include "core/i_logger.h"
#include "core/config_manager.h"

namespace vrinject {

class SubsystemContext {
public:
    static SubsystemContext& Get() {
        static SubsystemContext instance;
        return instance;
    }

    void Initialize(std::shared_ptr<ILogger> logger, std::shared_ptr<ConfigManager> config) {
        m_logger = std::move(logger);
        m_config = std::move(config);
    }

    void Shutdown() {
        m_logger.reset();
        m_config.reset();
    }

    ILogger* GetLogger() const { return m_logger.get(); }
    ConfigManager* GetConfig() const { return m_config.get(); }

private:
    SubsystemContext() = default;
    ~SubsystemContext() = default;

    std::shared_ptr<ILogger> m_logger;
    std::shared_ptr<ConfigManager> m_config;
};

} // namespace vrinject
