#include "frame_coordinator.h"
#include "../rendering/dx11_graphics_backend.h"
#include "../rendering/dx12_graphics_backend.h"
#include "../rendering/stereo_pipeline.h"
#include "../rendering/vulkan_graphics_backend.h"
#include "../rendering/vulkan_resource_state_tracker.h"
#include "dx11_resource_validator.h"
#include "logger.h"


#include "camera_delta_tracker.h"
#include "camera_lock_manager.h"
#include "camera_ranking_engine.h"
#include "camera_validator.h"
#include "candidate_collector.h"
#include "depth_candidate_collector.h"
#include "depth_delta_tracker.h"
#include "depth_lock_manager.h"
#include "engine_detector.h"


#include "stereo_camera_generator.h"
#include "stereo_frame_builder.h"

namespace vrinject {

void FrameCoordinator::OnPresentBegin(const RenderFrameSnapshot &snapshot) {
  if (m_frameActive) {
    LOG_WARN("FrameCoordinator: Re-entrant OnPresentBegin detected! Forcing "
             "EndFrame.");
    OnPresentEnd();
  }

  ScopedCpuTimer frameTimer(&m_cpuProfiler, CpuSegment::EntireFrame);

  m_currentSnapshot = snapshot;
  m_frameActive = true;
  m_globalFrameCounter++;

  m_stateMonitor.BeginFrame(m_globalFrameCounter);

  // DX11-specific profiling and validation - skip for DX12/Vulkan
  if (m_currentSnapshot.backend == GraphicsBackend::DX11) {
    m_gpuProfiler.BeginFrame(
        static_cast<ID3D11DeviceContext *>(m_currentSnapshot.nativeContext),
        m_globalFrameCounter);

    auto validation = Dx11ResourceValidator::Validate(m_currentSnapshot,
                                                      m_currentSnapshot.epoch);
    m_stateMonitor.UpdateDx11Health(validation.status ==
                                    Dx11ValidationStatus::VALID);
    if (validation.status != Dx11ValidationStatus::VALID) {
      CameraLockManager::Get().Reset();
      DepthLockManager::Get().OnDeviceLost();
      return;
    }
  } else if (m_currentSnapshot.backend == GraphicsBackend::DX12) {
    // DX12 path: skip DX11 validation, accept the snapshot as valid
    m_stateMonitor.UpdateDx11Health(true);
  } else if (m_currentSnapshot.backend == GraphicsBackend::Vulkan) {
    m_stateMonitor.UpdateDx11Health(true);
  }

  if (!m_engineDetected) {
    EngineDetector::Get().Detect();
    m_engineDetected = true;
  }

  if (m_globalFrameCounter <= 5 || m_globalFrameCounter % 300 == 0) {
    LOG_INFO("FrameCoordinator: Frame %llu begin (backend=%d, state=%d, %ux%u)",
             m_globalFrameCounter, (int)m_currentSnapshot.backend,
             (int)m_currentSnapshot.state,
             m_currentSnapshot.width, m_currentSnapshot.height);
  }

  // Camera and depth discovery - wrapped in try-catch because these subsystems
  // may crash on DX12 backends (they were designed for DX11). A crash here must
  // NOT prevent OpenXR initialization.
  CameraSnapshot camSnapshot;
  DepthSnapshot depthSnapshot;
  try {
    // 1. Collect candidates
    std::vector<CameraCandidate> candidates;
    {
      ScopedCpuTimer camTimer(&m_cpuProfiler, CpuSegment::CameraDiscovery);
      candidates = CandidateCollector::Get().GetAndClearCandidates();

      for (auto &c : candidates) {
        c.temporalScore = 0.5f;
        if (!CameraValidator::ValidateCandidate(c)) {
          c.valid = false;
        }
      }

      CameraCandidate bestCandidate;
      static uint64_t frameCounter = 0;
      frameCounter++;

      bool hasBest = CameraRankingEngine::RankCandidates(candidates, frameCounter,
                                                         bestCandidate);

      // Hand the lock manager this frame's colour resource before Update(), the same way
      // DepthLockManager::OnFrameEnd() is handed its resource context below. A
      // CameraCandidate carries only matrices, so without this the snapshot's
      // resourceIdentity stays null and CameraSnapshot::IsValid() can never be true.
      CameraLockManager::Get().SetResourceContext(m_currentSnapshot.BackBufferIdentity());

      if (hasBest) {
        CameraLockManager::Get().Update(bestCandidate, m_globalFrameCounter);
      } else {
        CameraCandidate empty = {};
        CameraLockManager::Get().Update(empty, m_globalFrameCounter);
      }

      camSnapshot = CameraLockManager::Get().GetSnapshot();
      m_stateMonitor.UpdateCameraHealth(camSnapshot.IsValid());
    }

    // ==============================================
    // DEPTH DISCOVERY SUBSYSTEM
    // ==============================================
    {
      ScopedCpuTimer depthTimer(&m_cpuProfiler, CpuSegment::DepthDiscovery);

      if (m_currentSnapshot.backend == GraphicsBackend::DX11) {
        DepthCandidateCollector::Get().OnFrameEnd(m_globalFrameCounter);
        DepthLockManager::Get().OnFrameEnd(
            m_globalFrameCounter,
            (uint32_t)Dx11LifecycleManager::Get().GetEpoch().deviceGeneration,
            (uint32_t)Dx11LifecycleManager::Get().GetEpoch().swapchainGeneration,
            m_currentSnapshot.width, m_currentSnapshot.height);
      } else {
        // DX12/Vulkan: skip DX11 depth discovery for now
        DepthCandidateCollector::Get().OnFrameEnd(m_globalFrameCounter);
        DepthLockManager::Get().OnFrameEnd(
            m_globalFrameCounter,
            (uint32_t)m_currentSnapshot.epoch.deviceGeneration,
            (uint32_t)m_currentSnapshot.epoch.swapchainGeneration,
            m_currentSnapshot.width, m_currentSnapshot.height);
      }

      depthSnapshot = DepthLockManager::Get().GetSnapshot();
      m_stateMonitor.UpdateDepthHealth(depthSnapshot.IsValid());
    }
  } catch (const std::exception& e) {
    LOG_WARN("FrameCoordinator: Camera/Depth discovery exception: %s (continuing to OpenXR)", e.what());
  } catch (...) {
    LOG_WARN("FrameCoordinator: Camera/Depth discovery unknown exception (continuing to OpenXR)");
  }

  // ==============================================
  // STEREO RENDERING PIPELINE
  // ==============================================
  LOG_INFO("FrameCoordinator: Entering stereo pipeline (frame %llu).", m_globalFrameCounter);

  // Lazy init managers
  if (!m_graphicsBackend) {
    if (m_currentSnapshot.backend == GraphicsBackend::DX11) {
      m_graphicsBackend = std::make_unique<DX11GraphicsBackend>();
      LOG_INFO("FrameCoordinator: Created DX11 graphics backend.");
    } else if (m_currentSnapshot.backend == GraphicsBackend::DX12) {
      m_graphicsBackend = std::make_unique<DX12GraphicsBackend>();
      LOG_INFO("FrameCoordinator: Created DX12 graphics backend.");
    } else if (m_currentSnapshot.backend == GraphicsBackend::Vulkan) {
      m_graphicsBackend = std::make_unique<vulkan::VulkanGraphicsBackend>();
      LOG_INFO("FrameCoordinator: Created Vulkan graphics backend.");
    } else {
      LOG_ERROR("FrameCoordinator: Unsupported graphics backend.");
      return;
    }
    m_graphicsBackend->Initialize(m_currentSnapshot.nativeDevice,
                                  m_currentSnapshot.nativeContext);
    LOG_INFO("FrameCoordinator: Graphics backend initialized (device=%p, context=%p).",
             m_currentSnapshot.nativeDevice, m_currentSnapshot.nativeContext);
    if (m_currentSnapshot.backend == GraphicsBackend::DX11) {
      m_gpuProfiler.Initialize(
          static_cast<ID3D11Device *>(m_currentSnapshot.nativeDevice));
    }
  }

  if (!m_oxrHealthMonitor) {
    m_oxrHealthMonitor = std::make_unique<openxr::OpenXRHealthMonitor>();
    LOG_INFO("FrameCoordinator: OpenXR health monitor created.");
  }
  if (!m_oxrRuntime) {
    LOG_INFO("FrameCoordinator: Creating OpenXR runtime manager...");
    m_oxrRuntime = std::make_unique<openxr::OpenXRRuntimeManager>(
        m_oxrHealthMonitor.get());
    bool oxrOk = m_oxrRuntime->Initialize("NexVR Engine", m_currentSnapshot.backend);
    if (oxrOk) {
      LOG_INFO("FrameCoordinator: OpenXR initialized successfully (state=%d).", (int)m_oxrRuntime->GetState());
    } else {
      LOG_ERROR("FrameCoordinator: OpenXR initialization FAILED!");
    }
  }

  // Pump OpenXR events
  m_oxrRuntime->PollEvents();

  if (m_oxrRuntime->GetState() == openxr::RuntimeState::SYSTEM_SELECTED) {
    LOG_INFO("FrameCoordinator: OpenXR state is SYSTEM_SELECTED, creating session (backend=%d)...", (int)m_currentSnapshot.backend);
    if (m_currentSnapshot.backend == GraphicsBackend::DX12) {
      // OpenXR REQUIRES xrGetD3D12GraphicsRequirementsKHR before xrCreateSession
      LUID adapterLuid{};
      if (!m_oxrRuntime->CheckDX12GraphicsRequirements(&adapterLuid)) {
        LOG_ERROR("FrameCoordinator: CheckDX12GraphicsRequirements failed!");
      } else {
        LOG_INFO("FrameCoordinator: DX12 graphics requirements satisfied (LUID: %08x:%08x).",
                 adapterLuid.HighPart, adapterLuid.LowPart);
      }
      bool sessionOk = m_oxrRuntime->CreateSessionDX12(
          static_cast<ID3D12Device *>(m_currentSnapshot.nativeDevice),
          static_cast<ID3D12CommandQueue *>(m_currentSnapshot.nativeContext));
      if (sessionOk) {
        LOG_INFO("FrameCoordinator: DX12 OpenXR session created successfully!");
      } else {
        LOG_ERROR("FrameCoordinator: DX12 OpenXR session creation FAILED!");
      }
    } else if (m_currentSnapshot.backend == GraphicsBackend::Vulkan) {
      m_oxrRuntime->CreateSessionVulkan(
          static_cast<VkInstance>(m_currentSnapshot.nativeInstance),
          static_cast<VkPhysicalDevice>(m_currentSnapshot.nativePhysicalDevice),
          static_cast<VkDevice>(m_currentSnapshot.nativeDevice),
          0, // queueFamilyIndex
          0  // queueIndex
      );
    } else {
      m_oxrRuntime->CreateSession(
          static_cast<ID3D11Device *>(m_currentSnapshot.nativeDevice));
    }
  }

  // Only attempt stereo rendering if both camera and depth are valid, and
  // OpenXR is ready
  bool xrReady =
      m_oxrRuntime->GetState() >= openxr::RuntimeState::SESSION_READY &&
      m_oxrRuntime->GetState() != openxr::RuntimeState::STOPPING &&
      m_oxrRuntime->GetState() != openxr::RuntimeState::STOPPED &&
      m_oxrRuntime->GetState() != openxr::RuntimeState::FAILED;

  m_lastCompatibility = CompatibilityScorer::Evaluate(
      camSnapshot, depthSnapshot, m_currentSnapshot.backend,
      EngineDetector::Get().GetDetection());

  if (!m_lastCompatibility.shouldAttemptStereo) {
    CompatibilityScorer::PostDiagnostic(m_lastCompatibility);
  }

  if (xrReady) {
    m_graphicsBackend->SetState(StereoRendererState::READY);

    if (m_graphicsBackend->GetState() == StereoRendererState::READY) {

      // ==============================================
      // OPENXR FRAME SUBMISSION & RENDER
      // ==============================================
      if (!m_oxrSwapchain) {
        DXGI_FORMAT targetFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        m_oxrSwapchain = std::make_unique<openxr::OpenXRSwapchainManager>(
            m_oxrHealthMonitor.get());
        m_oxrSwapchain->Initialize(
            m_oxrRuntime->GetSession(), targetFormat, m_currentSnapshot.width,
            m_currentSnapshot.height, m_currentSnapshot.backend);
      }
      if (!m_oxrSubmitter) {
        m_oxrSubmitter = std::make_unique<openxr::OpenXRFrameSubmitter>(
            m_oxrHealthMonitor.get());
      }

      // 2. Pure Math: Generate Eye Matrices
      StereoParams params;
      params.convergence = 0.5f;

      if (m_currentSnapshot.backend == GraphicsBackend::DX12) {
        ID3D12Resource *leftDest = nullptr;
        ID3D12Resource *rightDest = nullptr;

        {
          ScopedCpuTimer oxrTimer(&m_cpuProfiler, CpuSegment::OpenXrSubmission);
          if (m_oxrSubmitter->BeginAndAcquireDX12(m_oxrRuntime->GetSession(),
                                                  m_oxrSwapchain.get(),
                                                  leftDest, rightDest)) {
            static_cast<DX12GraphicsBackend *>(m_graphicsBackend.get())
                ->SetOpenXRSwapchainImages(leftDest, rightDest);

            m_graphicsBackend->RenderStereo(camSnapshot, depthSnapshot, params);

            m_oxrSubmitter->ReleaseAndEndDX12(
                m_oxrRuntime->GetSession(), m_oxrRuntime->GetReferenceSpace(),
                m_oxrSwapchain.get(), m_currentSnapshot.width,
                m_currentSnapshot.height, m_currentSnapshot.width,
                m_currentSnapshot.height);

            m_stateMonitor.UpdateStereoHealth(m_lastCompatibility.shouldAttemptStereo);
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
          if (m_oxrSubmitter->BeginAndAcquireVulkan(m_oxrRuntime->GetSession(),
                                                    m_oxrSwapchain.get(),
                                                    leftDest, rightDest)) {
            // Ensure the tracker knows these external images are in UNDEFINED
            // layout before the first use
            auto stateTracker = static_cast<vulkan::VulkanGraphicsBackend *>(
                                    m_graphicsBackend.get())
                                    ->GetStateTracker();
            if (leftDest) {
              stateTracker->ForceResourceState(
                  leftDest, VK_IMAGE_LAYOUT_UNDEFINED,
                  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0);
            }
            if (rightDest) {
              stateTracker->ForceResourceState(
                  rightDest, VK_IMAGE_LAYOUT_UNDEFINED,
                  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0);
            }

            static_cast<vulkan::VulkanGraphicsBackend *>(
                m_graphicsBackend.get())
                ->SetOpenXRSwapchainImages(leftDest, rightDest);

            m_graphicsBackend->RenderStereo(camSnapshot, depthSnapshot, params);

            m_oxrSubmitter->ReleaseAndEndVulkan(
                m_oxrRuntime->GetSession(), m_oxrRuntime->GetReferenceSpace(),
                m_oxrSwapchain.get(), m_currentSnapshot.width,
                m_currentSnapshot.height, m_currentSnapshot.width,
                m_currentSnapshot.height);

            m_stateMonitor.UpdateStereoHealth(true);
            m_stateMonitor.UpdateOpenXrHealth(true);
          } else {
            m_stateMonitor.UpdateOpenXrHealth(false);
          }
        }
      } else if (m_currentSnapshot.backend == GraphicsBackend::DX11) {
        // DX11 backend logic
        m_graphicsBackend->RenderStereo(camSnapshot, depthSnapshot, params);

        m_gpuProfiler.EndSegment(
            static_cast<ID3D11DeviceContext *>(m_currentSnapshot.nativeContext),
            GpuSegment::StereoCompute);
        m_gpuProfiler.BeginSegment(
            static_cast<ID3D11DeviceContext *>(m_currentSnapshot.nativeContext),
            GpuSegment::TextureCopies);

        m_stateMonitor.UpdateStereoHealth(true);

        {
          ScopedCpuTimer oxrTimer(&m_cpuProfiler, CpuSegment::OpenXrSubmission);

          m_gpuProfiler.BeginSegment(static_cast<ID3D11DeviceContext *>(
                                         m_currentSnapshot.nativeContext),
                                     GpuSegment::OpenXrSubmission);

          m_oxrSubmitter->SubmitStereoTextures(
              m_oxrRuntime->GetSession(), m_oxrRuntime->GetReferenceSpace(),
              m_oxrSwapchain.get(),
              static_cast<ID3D11DeviceContext *>(
                  m_currentSnapshot.nativeContext),
              static_cast<ID3D11Texture2D *>(
                  m_graphicsBackend->GetLeftEyeTexture()),
              static_cast<ID3D11Texture2D *>(
                  m_graphicsBackend->GetRightEyeTexture()));

          m_gpuProfiler.EndSegment(static_cast<ID3D11DeviceContext *>(
                                       m_currentSnapshot.nativeContext),
                                   GpuSegment::OpenXrSubmission);
          m_gpuProfiler.EndSegment(static_cast<ID3D11DeviceContext *>(
                                       m_currentSnapshot.nativeContext),
                                   GpuSegment::TextureCopies);
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
  if (!m_frameActive)
    return;

  // Resolve GPU queries and dashboard (DX11 only)
  if (m_graphicsBackend && m_currentSnapshot.backend == GraphicsBackend::DX11) {
    m_gpuProfiler.EndFrame(
        static_cast<ID3D11DeviceContext *>(m_currentSnapshot.nativeContext));
  }

  auto report = m_stateMonitor.EvaluateHealth();
  m_dashboard.Update(report, m_cpuProfiler, m_gpuProfiler);

  // Optional: Print to console every 1 second
  m_dashboard.PrintToConsole();

  m_frameActive = false;
}

} // namespace vrinject
