#pragma once
#include "render_frame_snapshot.h"

#include <memory>
#include "../rendering/igraphics_backend.h"
#include "../openxr/openxr_runtime_manager.h"
#include "../openxr/openxr_swapchain_manager.h"
#include "../openxr/openxr_frame_submitter.h"
#include "../openxr/openxr_health_monitor.h"

#include "runtime_state_monitor.h"
#include "performance_profiler.h"
#include "gpu_profiler.h"
#include "runtime_dashboard.h"
namespace vrinject {

class FrameCoordinator {
public:
    static FrameCoordinator& Get() {
        static FrameCoordinator instance;
        return instance;
    }

    void OnPresentBegin(const RenderFrameSnapshot& snapshot);
    void OnPresentEnd();
    
    // Test hook for DX11 tests
    IGraphicsBackend* GetGraphicsBackend() { return m_graphicsBackend.get(); }

private:
    FrameCoordinator() = default;
    
    RenderFrameSnapshot m_currentSnapshot;
    bool m_frameActive = false;
    
    std::unique_ptr<IGraphicsBackend> m_graphicsBackend;

    std::unique_ptr<openxr::OpenXRHealthMonitor> m_oxrHealthMonitor;
    std::unique_ptr<openxr::OpenXRRuntimeManager> m_oxrRuntime;
    std::unique_ptr<openxr::OpenXRSwapchainManager> m_oxrSwapchain;
    std::unique_ptr<openxr::OpenXRFrameSubmitter> m_oxrSubmitter;
    
    RuntimeStateMonitor m_stateMonitor;
    PerformanceProfiler m_cpuProfiler;
    GpuProfiler m_gpuProfiler;
    RuntimeDashboard m_dashboard;
    
    uint64_t m_globalFrameCounter = 0;
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
