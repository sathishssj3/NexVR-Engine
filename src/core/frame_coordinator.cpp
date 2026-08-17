#include "core/frame_coordinator.h"
#include <system_error>
#include "rendering/dx11/dx11_graphics_backend.h"
#include "rendering/dx12/dx12_graphics_backend.h"
#include "rendering/stereo/stereo_pipeline.h"
#include "rendering/vulkan/vulkan_graphics_backend.h"
#include "rendering/vulkan_resource_state_tracker.h"
#include "rendering/dx11/dx11_resource_validator.h"
#include "core/logger.h"

#include "memory_scanner/camera_delta_tracker.h"
#include "memory_scanner/page_scanner.h"
#include "heuristics/camera_lock_manager.h"
#include "heuristics/camera_ranking_engine.h"
#include "heuristics/camera_validator.h"
#include "heuristics/candidate_collector.h"
#include "heuristics/depth_candidate_collector.h"
#include "heuristics/depth_delta_tracker.h"
#include "heuristics/depth_lock_manager.h"
#include "core/diagnostic_context.h"
#include "core/subsystem_context.h"
#include "core/engine_detector.h"


#include "rendering/stereo/stereo_camera_generator.h"
#include "rendering/stereo/stereo_frame_builder.h"

#include "ai/backend/dx11_ai_backend.h"
#include "ai/backend/dx12_ai_backend.h"
#include "ai/backend/vulkan_ai_backend.h"

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
      SubsystemContext::Get().GetCameraLockManager()->Reset();
      SubsystemContext::Get().GetDepthLockManager()->OnDeviceLost();
      return;
    }
  } else if (m_currentSnapshot.backend == GraphicsBackend::DX12) {
    // DX12 path: skip DX11 validation, accept the snapshot as valid
    m_stateMonitor.UpdateDx11Health(true);
  } else if (m_currentSnapshot.backend == GraphicsBackend::Vulkan) {
    m_stateMonitor.UpdateDx11Health(true);
  }

  if (!m_engineDetected) {
    SubsystemContext::Get().GetEngineDetector()->Detect();
    m_engineDetected = true;
    
    // Phase 6: Initialize dynamic memory scanner
    if (SubsystemContext::Get().GetPageScanner()->Initialize()) {
        SubsystemContext::Get().GetPageScanner()->StartDynamicScan(90.0f); // Default 90 FOV
    }
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
      candidates = SubsystemContext::Get().GetCandidateCollector()->GetAndClearCandidates();

      for (auto &c : candidates) {
        c.temporalScore = 0.5f;
        if (!CameraValidator::ValidateCandidate(c)) {
          c.valid = false;
        }
      }

      // Phase 9: Feed headset tracking pose into the camera tracker
      m_deltaTracker.UpdateHeadsetPose(m_cachedHeadsetPose);

      // Phase 6: Poll dynamic candidates and track them
      m_deltaTracker.PollAndTrackCandidates();
      
      CameraCandidate bestCandidate = {};
      bool hasBest = false;
      
      Matrix4x4 dynamicCameraMatrix;
      if (m_deltaTracker.GetLockedCamera(dynamicCameraMatrix)) {
          // Dynamic scanner found a locked camera matrix!
          bestCandidate.view = dynamicCameraMatrix;
          bestCandidate.valid = true;
          bestCandidate.temporalScore = 1.0f;
          bestCandidate.confidence = 100.0f; // Force selection
          hasBest = true;
      } else {
          // Fall back to static hardcoded hooks/heuristics
          static uint64_t frameCounter = 0;
          frameCounter++;
          hasBest = CameraRankingEngine::RankCandidates(candidates, frameCounter, bestCandidate);
      }

      // Hand the lock manager this frame's colour resource before Update(), the same way
      // DepthLockManager::OnFrameEnd() is handed its resource context below. A
      // CameraCandidate carries only matrices, so without this the snapshot's
      // resourceIdentity stays null and CameraSnapshot::IsValid() can never be true.
      SubsystemContext::Get().GetCameraLockManager()->SetResourceContext(m_currentSnapshot.BackBufferIdentity());

      if (hasBest) {
        SubsystemContext::Get().GetCameraLockManager()->Update(bestCandidate, m_globalFrameCounter);
      } else {
        CameraCandidate empty = {};
        SubsystemContext::Get().GetCameraLockManager()->Update(empty, m_globalFrameCounter);
      }

      camSnapshot = SubsystemContext::Get().GetCameraLockManager()->GetSnapshot();
      m_stateMonitor.UpdateCameraHealth(camSnapshot.IsValid());
    }

    // ==============================================
    // DEPTH DISCOVERY SUBSYSTEM
    // ==============================================
    {
      ScopedCpuTimer depthTimer(&m_cpuProfiler, CpuSegment::DepthDiscovery);

      if (m_currentSnapshot.backend == GraphicsBackend::DX11) {
        SubsystemContext::Get().GetDepthCandidateCollector()->OnFrameEnd(m_globalFrameCounter);
        SubsystemContext::Get().GetDepthLockManager()->OnFrameEnd(
            m_globalFrameCounter,
            (uint32_t)Dx11LifecycleManager::Get().GetEpoch().deviceGeneration,
            (uint32_t)Dx11LifecycleManager::Get().GetEpoch().swapchainGeneration,
            m_currentSnapshot.width, m_currentSnapshot.height);
      } else {
        // DX12/Vulkan: skip DX11 depth discovery for now
        SubsystemContext::Get().GetDepthCandidateCollector()->OnFrameEnd(m_globalFrameCounter);
        SubsystemContext::Get().GetDepthLockManager()->OnFrameEnd(
            m_globalFrameCounter,
            (uint32_t)m_currentSnapshot.epoch.deviceGeneration,
            (uint32_t)m_currentSnapshot.epoch.swapchainGeneration,
            m_currentSnapshot.width, m_currentSnapshot.height);
      }

      depthSnapshot = SubsystemContext::Get().GetDepthLockManager()->GetSnapshot();
      m_stateMonitor.UpdateDepthHealth(depthSnapshot.IsValid());
    }
  } catch (const std::system_error& e) {
    LOG_WARN("FrameCoordinator: Camera/Depth discovery system exception: %s (code: %d) (continuing to OpenXR)", e.what(), e.code().value());
  } catch (const std::exception& e) {
    LOG_WARN("FrameCoordinator: Camera/Depth discovery exception: %s (continuing to OpenXR)", e.what());
  } catch (...) {
    LOG_WARN("FrameCoordinator: Camera/Depth discovery unknown exception. Check VEH observer logs for SEH code. (continuing to OpenXR)");
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

  // Initialize AI Scheduler
  if (!m_aiScheduler) {
    std::unique_ptr<ai::IAIBackend> aiBackend;
    if (m_currentSnapshot.backend == GraphicsBackend::DX11) {
      aiBackend = std::make_unique<ai::DX11AIBackend>();
    } else if (m_currentSnapshot.backend == GraphicsBackend::DX12) {
      aiBackend = std::make_unique<ai::DX12AIBackend>();
    } else if (m_currentSnapshot.backend == GraphicsBackend::Vulkan) {
      aiBackend = std::make_unique<ai::VulkanAIBackend>();
    }
    
    if (aiBackend) {
        m_aiScheduler = std::make_unique<ai::AIScheduler>(std::move(aiBackend));
        LOG_INFO("FrameCoordinator: AI Scheduler initialized.");
    } else {
        LOG_WARN("FrameCoordinator: Could not initialize AI Scheduler (Unsupported backend)");
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
    if (!m_graphicsBackend->CreateOpenXRSession(m_oxrRuntime.get(), m_currentSnapshot)) {
        LOG_ERROR("FrameCoordinator: OpenXR session creation FAILED!");
    } else {
        LOG_INFO("FrameCoordinator: OpenXR session created successfully. Initializing Input Manager...");
        m_inputManager.Initialize(m_oxrRuntime->GetInstance(), m_oxrRuntime->GetSession());
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
      SubsystemContext::Get().GetEngineDetector()->GetDetection());

  if (!m_lastCompatibility.shouldAttemptStereo) {
    CompatibilityScorer::PostDiagnostic(m_lastCompatibility);
  }

  if (xrReady) {
    // Phase 9: Update virtual gamepad input from VR controllers
    m_inputManager.Update(m_oxrRuntime->GetSession());

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

      if (m_aiScheduler) {
          ai::AIJob aiJob;
          aiJob.frameId = m_globalFrameCounter;
          aiJob.colorTarget = camSnapshot.resourceIdentity.nativeHandle;
          aiJob.depthTarget = depthSnapshot.identity.nativeHandle;
          aiJob.width = m_currentSnapshot.width;
          aiJob.height = m_currentSnapshot.height;
          m_aiScheduler->PushJob(aiJob);
      }

      m_graphicsBackend->SubmitStereoFrame(
          m_oxrRuntime.get(),
          m_oxrSwapchain.get(),
          m_oxrSubmitter.get(),
          m_currentSnapshot,
          camSnapshot,
          depthSnapshot,
          params,
          m_stateMonitor,
          m_cpuProfiler,
          m_gpuProfiler,
          m_lastCompatibility.shouldAttemptStereo,
          m_aiScheduler ? m_aiScheduler->GetLatestUIMask() : nullptr
      );

      // Cache the headset pose for the next frame's injection
      m_cachedHeadsetPose = m_currentSnapshot.leftPose;

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
