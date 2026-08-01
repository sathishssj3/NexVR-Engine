#include "frame_coordinator.h"
#include "logger.h"
#include "dx11_resource_validator.h"
#include "../rendering/dx11_graphics_backend.h"
#include "../rendering/dx12_graphics_backend.h"
#include "../rendering/vulkan_graphics_backend.h"
#include "../rendering/vulkan_resource_state_tracker.h"
#include "../rendering/stereo_pipeline.h"

#include "candidate_collector.h"
#include "camera_delta_tracker.h"
#include "camera_ranking_engine.h"
#include "camera_validator.h"
#include "camera_lock_manager.h"
#include "depth_candidate_collector.h"
#include "depth_lock_manager.h"
#include "depth_delta_tracker.h"

#include "stereo_camera_generator.h"
#include "stereo_frame_builder.h"

namespace vrinject {

void FrameCoordinator::OnPresentBegin(const RenderFrameSnapshot& snapshot) {
    if (m_frameActive) {
        LOG_WARN("FrameCoordinator: Re-entrant OnPresentBegin detected! Forcing EndFrame.");
        OnPresentEnd();
    }
    
    ScopedCpuTimer frameTimer(&m_cpuProfiler, CpuSegment::EntireFrame);
    
    m_currentSnapshot = snapshot;
    m_frameActive = true;
    m_globalFrameCounter++;
    
    m_stateMonitor.BeginFrame(m_globalFrameCounter);
    m_gpuProfiler.BeginFrame(static_cast<ID3D11DeviceContext*>(m_currentSnapshot.nativeContext), m_globalFrameCounter);
    
    auto validation = Dx11ResourceValidator::Validate(m_currentSnapshot, m_currentSnapshot.epoch);
    m_stateMonitor.UpdateDx11Health(validation.status == Dx11ValidationStatus::VALID);
    if (validation.status != Dx11ValidationStatus::VALID) {
        // VR subsystems should bypass entirely if the snapshot is invalid (e.g. DEGRADED mode)
        CameraLockManager::Get().Reset();
        DepthLockManager::Get().OnDeviceLost();
        return;
    }

    // 1. Collect candidates
    std::vector<CameraCandidate> candidates;
    CameraSnapshot camSnapshot;
    {
        ScopedCpuTimer camTimer(&m_cpuProfiler, CpuSegment::CameraDiscovery);
        candidates = CandidateCollector::Get().GetAndClearCandidates();
    
    // 2. Track delta/motion (not fully implemented with persistent history here yet, but we will rank them)
    // For a real production delta tracker, we need to maintain previous frames' candidates.
    // We can do a simplified pass here for Sprint 3.3.
    for (auto& c : candidates) {
        // Delta tracker would need the previous candidate with the same ID
        // For now, assume it's fresh or gets default motion score.
        c.temporalScore = 0.5f; 
        
        // 3. Validate (reject UI/static)
        if (!CameraValidator::ValidateCandidate(c)) {
            c.valid = false;
        }
    }

    // 4. Rank
    CameraCandidate bestCandidate;
    static uint64_t frameCounter = 0;
    frameCounter++;

    bool hasBest = CameraRankingEngine::RankCandidates(candidates, frameCounter, bestCandidate);

    // 5. State Machine Lock
    if (hasBest) {
        CameraLockManager::Get().Update(bestCandidate, m_globalFrameCounter);
    } else {
        CameraCandidate empty = {};
        CameraLockManager::Get().Update(empty, m_globalFrameCounter);
    }
    
    // 6. Camera Snapshot ready for XR/Stereo later
    camSnapshot = CameraLockManager::Get().GetSnapshot();
    m_stateMonitor.UpdateCameraHealth(camSnapshot.IsValid());
    } // End Camera scope

    // ==============================================
    // DEPTH DISCOVERY SUBSYSTEM
    // ==============================================
    DepthSnapshot depthSnapshot;
    {
        ScopedCpuTimer depthTimer(&m_cpuProfiler, CpuSegment::DepthDiscovery);
        DepthCandidateCollector::Get().OnFrameEnd(m_globalFrameCounter);
        DepthLockManager::Get().OnFrameEnd(
            m_globalFrameCounter,
            (uint32_t)Dx11LifecycleManager::Get().GetEpoch().deviceGeneration,
            (uint32_t)Dx11LifecycleManager::Get().GetEpoch().swapchainGeneration,
            m_currentSnapshot.width,
            m_currentSnapshot.height
        );
        
        depthSnapshot = DepthLockManager::Get().GetSnapshot();
        m_stateMonitor.UpdateDepthHealth(depthSnapshot.IsValid());
    }

    // ==============================================
    // STEREO RENDERING PIPELINE
    // ==============================================
    
    // Lazy init managers
    if (!m_graphicsBackend) {
        if (m_currentSnapshot.backend == GraphicsBackend::DX11) {
            m_graphicsBackend = std::make_unique<DX11GraphicsBackend>();
        } else if (m_currentSnapshot.backend == GraphicsBackend::Vulkan) {
            m_graphicsBackend = std::make_unique<vulkan::VulkanGraphicsBackend>();
        } else {
            LOG_ERROR("FrameCoordinator: Unsupported graphics backend.");
            return;
        }
        m_graphicsBackend->Initialize(m_currentSnapshot.nativeDevice, m_currentSnapshot.nativeContext);
        m_gpuProfiler.Initialize(static_cast<ID3D11Device*>(m_currentSnapshot.nativeDevice));
    }

    if (!m_oxrHealthMonitor) {
        m_oxrHealthMonitor = std::make_unique<openxr::OpenXRHealthMonitor>();
    }
    if (!m_oxrRuntime) {
        m_oxrRuntime = std::make_unique<openxr::OpenXRRuntimeManager>(m_oxrHealthMonitor.get());
        m_oxrRuntime->Initialize("NexVR", m_currentSnapshot.backend);
    }
    
    // Pump OpenXR events
    m_oxrRuntime->PollEvents();

    if (m_oxrRuntime->GetState() == openxr::RuntimeState::SYSTEM_SELECTED) {
        if (m_currentSnapshot.backend == GraphicsBackend::DX12) {
            m_oxrRuntime->CreateSessionDX12(
                static_cast<ID3D12Device*>(m_currentSnapshot.nativeDevice),
                static_cast<ID3D12CommandQueue*>(m_currentSnapshot.nativeContext)
            );
        } else if (m_currentSnapshot.backend == GraphicsBackend::Vulkan) {
            // Queue family index logic depends on snapshot or backend. 
            // In our mock, queueFamilyIndex is 0.
            m_oxrRuntime->CreateSessionVulkan(
                static_cast<VkInstance>(m_currentSnapshot.nativeInstance),
                static_cast<VkPhysicalDevice>(m_currentSnapshot.nativePhysicalDevice),
                static_cast<VkDevice>(m_currentSnapshot.nativeDevice),
                0, // queueFamilyIndex
                0  // queueIndex
            );
        } else {
            m_oxrRuntime->CreateSession(static_cast<ID3D11Device*>(m_currentSnapshot.nativeDevice));
        }
    }

    // Only attempt stereo rendering if both camera and depth are valid, and OpenXR is ready
    bool xrReady = m_oxrRuntime->GetState() >= openxr::RuntimeState::SESSION_READY &&
                   m_oxrRuntime->GetState() != openxr::RuntimeState::STOPPING &&
                   m_oxrRuntime->GetState() != openxr::RuntimeState::STOPPED &&
                   m_oxrRuntime->GetState() != openxr::RuntimeState::FAILED;

    if (camSnapshot.IsValid() && depthSnapshot.IsValid() && xrReady) {
        
        m_graphicsBackend->SetState(StereoRendererState::READY);
        
        if (m_graphicsBackend->GetState() == StereoRendererState::READY) {
            
            // ==============================================
            // OPENXR FRAME SUBMISSION & RENDER
            // ==============================================
            if (!m_oxrSwapchain) {
                DXGI_FORMAT targetFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
                m_oxrSwapchain = std::make_unique<openxr::OpenXRSwapchainManager>(m_oxrHealthMonitor.get());
                m_oxrSwapchain->Initialize(
                    m_oxrRuntime->GetSession(), 
                    targetFormat, 
                    m_currentSnapshot.width, 
                    m_currentSnapshot.height,
                    m_currentSnapshot.backend
                );
            }
            if (!m_oxrSubmitter) {
                m_oxrSubmitter = std::make_unique<openxr::OpenXRFrameSubmitter>(m_oxrHealthMonitor.get());
            }

            // 2. Pure Math: Generate Eye Matrices
            StereoParams params;
            params.convergence = 0.5f;

            if (m_currentSnapshot.backend == GraphicsBackend::DX12) {
                ID3D12Resource* leftDest = nullptr;
                ID3D12Resource* rightDest = nullptr;
                
                {
                    ScopedCpuTimer oxrTimer(&m_cpuProfiler, CpuSegment::OpenXrSubmission);
                    if (m_oxrSubmitter->BeginAndAcquireDX12(
                            m_oxrRuntime->GetSession(),
                            m_oxrSwapchain.get(),
                            leftDest,
                            rightDest)) 
                    {
                        static_cast<DX12GraphicsBackend*>(m_graphicsBackend.get())->SetOpenXRSwapchainImages(leftDest, rightDest);
                        
                        m_graphicsBackend->RenderStereo(camSnapshot, depthSnapshot, params);
                        
                        m_oxrSubmitter->ReleaseAndEndDX12(
                            m_oxrRuntime->GetSession(),
                            m_oxrRuntime->GetReferenceSpace(),
                            m_oxrSwapchain.get(),
                            m_currentSnapshot.width, m_currentSnapshot.height,
                            m_currentSnapshot.width, m_currentSnapshot.height
                        );
                        
                        m_stateMonitor.UpdateStereoHealth(true);
                        m_stateMonitor.UpdateOpenXrHealth(true);
                    } else {
                        m_stateMonitor.UpdateOpenXrHealth(false);
                    }
                }
            } else if (m_currentSnapshot.backend == GraphicsBackend::Vulkan) {
                VkImage leftDest = VK_NULL_HANDLE;
                VkImage rightDest = VK_NULL_HANDLE;
                
                {
                    ScopedCpuTimer oxrTimer(&m_cpuProfiler, CpuSegment::OpenXrSubmission);
                    if (m_oxrSubmitter->BeginAndAcquireVulkan(
                            m_oxrRuntime->GetSession(),
                            m_oxrSwapchain.get(),
                            leftDest,
                            rightDest)) 
                    {
                        // Ensure the tracker knows these external images are in UNDEFINED layout before the first use
                        auto stateTracker = static_cast<vulkan::VulkanGraphicsBackend*>(m_graphicsBackend.get())->GetStateTracker();
                        if (leftDest) {
                            stateTracker->ForceResourceState(leftDest, VK_IMAGE_LAYOUT_UNDEFINED, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0);
                        }
                        if (rightDest) {
                            stateTracker->ForceResourceState(rightDest, VK_IMAGE_LAYOUT_UNDEFINED, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0);
                        }

                        static_cast<vulkan::VulkanGraphicsBackend*>(m_graphicsBackend.get())->SetOpenXRSwapchainImages(leftDest, rightDest);
                        
                        m_graphicsBackend->RenderStereo(camSnapshot, depthSnapshot, params);
                        
                        m_oxrSubmitter->ReleaseAndEndVulkan(
                            m_oxrRuntime->GetSession(),
                            m_oxrRuntime->GetReferenceSpace(),
                            m_oxrSwapchain.get(),
                            m_currentSnapshot.width, m_currentSnapshot.height,
                            m_currentSnapshot.width, m_currentSnapshot.height
                        );
                        
                        m_stateMonitor.UpdateStereoHealth(true);
                        m_stateMonitor.UpdateOpenXrHealth(true);
                    } else {
                        m_stateMonitor.UpdateOpenXrHealth(false);
                    }
                }
            } else {
                // DX11 or other backend logic
                m_graphicsBackend->RenderStereo(camSnapshot, depthSnapshot, params);

                m_gpuProfiler.EndSegment(static_cast<ID3D11DeviceContext*>(m_currentSnapshot.nativeContext), GpuSegment::StereoCompute);
                m_gpuProfiler.BeginSegment(static_cast<ID3D11DeviceContext*>(m_currentSnapshot.nativeContext), GpuSegment::TextureCopies);
                
                m_stateMonitor.UpdateStereoHealth(true);
                
                {
                    ScopedCpuTimer oxrTimer(&m_cpuProfiler, CpuSegment::OpenXrSubmission);
                    
                    m_gpuProfiler.BeginSegment(static_cast<ID3D11DeviceContext*>(m_currentSnapshot.nativeContext), GpuSegment::OpenXrSubmission);
                    
                    m_oxrSubmitter->SubmitStereoTextures(
                        m_oxrRuntime->GetSession(),
                        m_oxrRuntime->GetReferenceSpace(),
                        m_oxrSwapchain.get(),
                        static_cast<ID3D11DeviceContext*>(m_currentSnapshot.nativeContext),
                        static_cast<ID3D11Texture2D*>(m_graphicsBackend->GetLeftEyeTexture()),
                        static_cast<ID3D11Texture2D*>(m_graphicsBackend->GetRightEyeTexture())
                    );
                    
                    m_gpuProfiler.EndSegment(static_cast<ID3D11DeviceContext*>(m_currentSnapshot.nativeContext), GpuSegment::OpenXrSubmission);
                    m_gpuProfiler.EndSegment(static_cast<ID3D11DeviceContext*>(m_currentSnapshot.nativeContext), GpuSegment::TextureCopies);
                }
                m_stateMonitor.UpdateOpenXrHealth(true);
            }
            m_stateMonitor.UpdateOpenXrHealth(true);
        } else {
            m_stateMonitor.UpdateStereoHealth(false);
            m_stateMonitor.UpdateOpenXrHealth(false);
        }
    } else {
        m_graphicsBackend->SetState(StereoRendererState::DEGRADED);
        m_stateMonitor.UpdateStereoHealth(false);
        m_stateMonitor.UpdateOpenXrHealth(false);
    }
}

void FrameCoordinator::OnPresentEnd() {
    if (!m_frameActive) return;
    
    // Resolve GPU queries and dashboard
    if (m_graphicsBackend) {
        m_gpuProfiler.EndFrame(static_cast<ID3D11DeviceContext*>(m_currentSnapshot.nativeContext));
    }
    
    auto report = m_stateMonitor.EvaluateHealth();
    m_dashboard.Update(report, m_cpuProfiler, m_gpuProfiler);
    
    // Optional: Print to console every 1 second
    m_dashboard.PrintToConsole();
    
    m_frameActive = false;
}

} // namespace vrinject
