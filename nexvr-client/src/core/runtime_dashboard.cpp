#include "core/runtime_dashboard.h"
#include "core/logger.h"
#include <chrono>
#include <iostream>
#include <iomanip>

namespace vrinject {

RuntimeDashboard::RuntimeDashboard() {
    for (size_t i = 0; i < static_cast<size_t>(CpuSegment::COUNT); ++i) {
        m_cpuStats[i] = {};
    }
}

void RuntimeDashboard::Update(
    const RuntimeHealthReport& health,
    const PerformanceProfiler& cpuProfiler,
    const GpuProfiler& gpuProfiler
) {
    m_lastHealth = health;

    for (size_t i = 0; i < static_cast<size_t>(CpuSegment::COUNT); ++i) {
        m_cpuStats[i] = cpuProfiler.GetStats(static_cast<CpuSegment>(i));
    }

    uint64_t gpuFrame = 0;
    std::array<GpuTimingResult, static_cast<uint32_t>(GpuSegment::COUNT)> gpuResults;
    if (const_cast<GpuProfiler&>(gpuProfiler).TryGetTimings(gpuFrame, gpuResults)) {
        m_lastGpuFrame = gpuFrame;
        m_gpuResults = gpuResults;
    }
}

void RuntimeDashboard::PrintToConsole() const {
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    if (now - m_lastPrintTimeMs < 1000) {
        return; // Print once a second max
    }
    const_cast<RuntimeDashboard*>(this)->m_lastPrintTimeMs = now;

    const char* stateStr = "UNKNOWN";
    switch (m_lastHealth.state) {
        case RuntimeHealthState::RUNNING: stateStr = "RUNNING"; break;
        case RuntimeHealthState::DEGRADED: stateStr = "DEGRADED"; break;
        case RuntimeHealthState::RECOVERING: stateStr = "RECOVERING"; break;
        case RuntimeHealthState::FAILED: stateStr = "FAILED"; break;
    }

    std::cout << "\n=== [NexVR Dashboard] ========================\n";
    std::cout << "State: " << stateStr 
              << " | Frame: " << m_lastHealth.frameNumber 
              << " | Dropped: " << m_lastHealth.droppedFrames << "\n";
    std::cout << "Health: DX11[" << (m_lastHealth.dx11Healthy?"OK":"FAIL") << "] "
              << "Cam[" << (m_lastHealth.cameraHealthy?"OK":"FAIL") << "] "
              << "Dep[" << (m_lastHealth.depthHealthy?"OK":"FAIL") << "] "
              << "OXR[" << (m_lastHealth.openxrHealthy?"OK":"FAIL") << "]\n";
    
    std::cout << "--- CPU Latencies (P99 ms) ---\n";
    std::cout << "Total: " << std::fixed << std::setprecision(3) << m_cpuStats[static_cast<size_t>(CpuSegment::EntireFrame)].p99Ms << " ms\n";
    std::cout << "  Camera: " << m_cpuStats[static_cast<size_t>(CpuSegment::CameraDiscovery)].p99Ms << " ms\n";
    std::cout << "  Depth:  " << m_cpuStats[static_cast<size_t>(CpuSegment::DepthDiscovery)].p99Ms << " ms\n";
    std::cout << "  Stereo: " << m_cpuStats[static_cast<size_t>(CpuSegment::StereoGeneration)].p99Ms << " ms\n";
    std::cout << "  OXR Submit: " << m_cpuStats[static_cast<size_t>(CpuSegment::OpenXrSubmission)].p99Ms << " ms\n";

    std::cout << "--- GPU Latencies (Latest ms) ---\n";
    if (m_lastGpuFrame > 0) {
        auto printGpu = [&](const char* name, GpuSegment seg) {
            auto result = m_gpuResults[static_cast<size_t>(seg)];
            std::cout << "  " << name << ": ";
            if (result.valid) std::cout << result.ms << " ms\n";
            else std::cout << "N/A\n";
        };
        printGpu("Stereo Compute", GpuSegment::StereoCompute);
        printGpu("Texture Copies", GpuSegment::TextureCopies);
        printGpu("OXR Submission", GpuSegment::OpenXrSubmission);
    } else {
        std::cout << "  (Pending resolution...)\n";
    }
    std::cout << "==============================================\n";
}

void RuntimeDashboard::LogToSystem() const {
    // Similar to PrintToConsole but routed through logger.h
}

} // namespace vrinject
