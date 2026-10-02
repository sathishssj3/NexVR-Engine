#pragma once

#include "ai/backend/ai_backend.h"
#include <vulkan/vulkan.h>
#include <chrono>

namespace vrinject {
namespace ai {

class VulkanAIBackend : public IAIBackend {
public:
    VulkanAIBackend();
    ~VulkanAIBackend() override;

    bool Initialize() override;
    void Shutdown() override;

    bool CreateResources() override;
    void DestroyResources() override;

    uint64_t SubmitInference(const AIJob& job) override;
    bool PollCompletion(uint64_t jobId) override;
    void Synchronize(uint64_t jobId) override;

    MemoryUsage GetMemoryUsage() const override;
    TelemetryData GetTelemetry() const override;
    void* GetUIMask() override { return nullptr; }
    const char* GetName() const override { return "Vulkan"; }

private:
    // Dummy variables for now until fully wired
    uint64_t m_currentJobId = 0;
    TelemetryData m_telemetry;
};

} // namespace ai
} // namespace vrinject
