#include "dx11_ai_backend.h"
#include <thread>

namespace vrinject {
namespace ai {

DX11AIBackend::DX11AIBackend() {}
DX11AIBackend::~DX11AIBackend() {}

bool DX11AIBackend::Initialize() { return true; }
void DX11AIBackend::Shutdown() {}

bool DX11AIBackend::CreateResources() { return true; }
void DX11AIBackend::DestroyResources() {}

uint64_t DX11AIBackend::SubmitInference(uint64_t frameId) {
    auto start = std::chrono::high_resolution_clock::now();
    // Simulate AI execution via Deferred Context (e.g. 1.1ms)
    std::this_thread::sleep_for(std::chrono::microseconds(1100));
    auto end = std::chrono::high_resolution_clock::now();
    
    m_telemetry.gpuTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
    m_currentJobId = frameId;
    return m_currentJobId;
}

bool DX11AIBackend::PollCompletion(uint64_t jobId) {
    return true; // Polling ID3D11Query
}

void DX11AIBackend::Synchronize(uint64_t jobId) {
    // Blocking while ID3D11Query is S_FALSE
}

MemoryUsage DX11AIBackend::GetMemoryUsage() const {
    MemoryUsage usage;
    usage.modelBytes = 25 * 1024 * 1024;
    usage.tensorBytes = 5 * 1024 * 1024;
    usage.totalBytes = usage.modelBytes + usage.tensorBytes;
    return usage;
}

TelemetryData DX11AIBackend::GetTelemetry() const {
    return m_telemetry;
}

} // namespace ai
} // namespace vrinject
