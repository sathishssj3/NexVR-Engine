#pragma once
#include "heuristics/render_frame_snapshot.h"

#include <memory>
#include "rendering/igraphics_backend.h"
#include "openxr/openxr_runtime_manager.h"
#include "openxr/openxr_swapchain_manager.h"
#include "openxr/openxr_frame_submitter.h"
#include "openxr/openxr_health_monitor.h"

#include "core/runtime_state_monitor.h"
#include "core/performance_profiler.h"
#include "core/gpu_profiler.h"
#include "core/runtime_dashboard.h"
#include "core/compatibility_scorer.h"
#include "memory_scanner/camera_delta_tracker.h"
#include "ai/ai_scheduler.h"
#include "hooks/input_manager.h"

namespace vrinject {

class FrameCoordinator {
public:
    FrameCoordinator() = default;

    void OnPresentBegin(const RenderFrameSnapshot& snapshot);
    void OnPresentEnd();
    
    // Test hook for DX11 tests
    IGraphicsBackend* GetGraphicsBackend() { return m_graphicsBackend.get(); }

private:
    
    RenderFrameSnapshot m_currentSnapshot;
    bool m_frameActive = false;
    
    std::unique_ptr<IGraphicsBackend> m_graphicsBackend;

    std::unique_ptr<openxr::OpenXRHealthMonitor> m_oxrHealthMonitor;
    std::unique_ptr<openxr::OpenXRRuntimeManager> m_oxrRuntime;
    std::unique_ptr<openxr::OpenXRSwapchainManager> m_oxrSwapchain;
    std::unique_ptr<openxr::OpenXRFrameSubmitter> m_oxrSubmitter;
    std::unique_ptr<ai::AIScheduler> m_aiScheduler;
    InputManager m_inputManager;
    
    RuntimeStateMonitor m_stateMonitor;
    CameraDeltaTracker m_deltaTracker;
    PerformanceProfiler m_cpuProfiler;
    GpuProfiler m_gpuProfiler;
    RuntimeDashboard m_dashboard;
    CompatibilityScore m_lastCompatibility;
    bool m_engineDetected = false;
    
    uint64_t m_globalFrameCounter = 0;
    
    // Cached headset pose from the previous frame to feed back into the engine
    XrPosef m_cachedHeadsetPose = {{0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f}};
};

// RAII Guard to guarantee OnPresentEnd is fired
class ScopedFrame {
public:
    ScopedFrame(FrameCoordinator& coordinator, const RenderFrameSnapshot& snapshot) 
        : m_coordinator(coordinator) {
        m_coordinator.OnPresentBegin(snapshot);
    }
    
    ~ScopedFrame() {
        m_coordinator.OnPresentEnd();
    }
    
    // Prevent copying/moving to guarantee strict scope
    ScopedFrame(const ScopedFrame&) = delete;
    ScopedFrame& operator=(const ScopedFrame&) = delete;
    ScopedFrame(ScopedFrame&&) = delete;
    ScopedFrame& operator=(ScopedFrame&&) = delete;

private:
    FrameCoordinator& m_coordinator;
};

} // namespace vrinject
