#pragma once

#include "ai_backend.h"
#include <chrono>

namespace vrinject {
namespace ai {

class DX11AIBackend : public IAIBackend {
public:
    DX11AIBackend();
    ~DX11AIBackend() override;

    bool Initialize() override;
    void Shutdown() override;

    bool CreateResources() override;
    void DestroyResources() override;

    uint64_t SubmitInference(uint64_t frameId) override;
    bool PollCompletion(uint64_t jobId) override;
    void Synchronize(uint64_t jobId) override;

    MemoryUsage GetMemoryUsage() const override;
    TelemetryData GetTelemetry() const override;
    const char* GetName() const override { return "DirectX 11"; }

private:
    uint64_t m_currentJobId = 0;
    TelemetryData m_telemetry;
};

} // namespace ai
} // namespace vrinject
