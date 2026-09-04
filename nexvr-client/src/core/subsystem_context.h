#pragma once

#include <memory>
#include "core/i_logger.h"
#include "core/config_manager.h"
#include "core/diagnostic_context.h"
#include "core/capability_registry.h"
#include "core/sprint_compatibility_logger.h"
#include "heuristics/matrix_classifier.h"
#include "heuristics/temporal_camera_filter.h"
#include "heuristics/temporal_depth_filter.h"
#include "heuristics/candidate_collector.h"
#include "heuristics/depth_candidate_collector.h"
#include "heuristics/camera_lock_manager.h"
#include "heuristics/depth_lock_manager.h"
#include "memory_scanner/signature_scanner.h"
#include "memory_scanner/pointer_chain_resolver.h"
#include "memory_scanner/page_scanner.h"
#include "core/engine_detector.h"
#include "core/engine_scanners/universal_scanner.h"
#include "core/frame_coordinator.h"

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
        m_diagnosticContext = std::make_unique<DiagnosticContext>();
        m_capabilityRegistry = std::make_unique<CapabilityRegistry>();
        m_sprintCompatibilityLogger = std::make_unique<SprintCompatibilityLogger>();
        m_matrixClassifier = std::make_unique<MatrixClassifier>();
        m_temporalCameraFilter = std::make_unique<TemporalCameraFilter>();
        m_temporalDepthFilter = std::make_unique<TemporalDepthFilter>();
        m_candidateCollector = std::make_unique<CandidateCollector>();
        m_depthCandidateCollector = std::make_unique<DepthCandidateCollector>();
        m_cameraLockManager = std::make_unique<CameraLockManager>();
        m_depthLockManager = std::make_unique<DepthLockManager>();
        
        m_signatureScanner = std::make_unique<SignatureScanner>();
        m_pointerChainResolver = std::make_unique<PointerChainResolver>();
        m_pageScanner = std::make_unique<PageScanner>();
        
        m_engineDetector = std::make_unique<EngineDetector>();
        m_universalScanner = std::make_unique<engine_scanners::UniversalScanner>();
        m_frameCoordinator = std::make_unique<FrameCoordinator>();
    }

    void Shutdown() {
        m_frameCoordinator.reset();
        m_universalScanner.reset();
        m_engineDetector.reset();

        m_pageScanner.reset();
        m_pointerChainResolver.reset();
        m_signatureScanner.reset();
        
        m_depthLockManager.reset();
        m_cameraLockManager.reset();
        m_depthCandidateCollector.reset();
        m_candidateCollector.reset();
        m_temporalDepthFilter.reset();
        m_temporalCameraFilter.reset();
        m_matrixClassifier.reset();
        m_sprintCompatibilityLogger.reset();
        m_capabilityRegistry.reset();
        m_diagnosticContext.reset();
        m_logger.reset();
        m_config.reset();
    }

    ILogger* GetLogger() const { return m_logger.get(); }
    ConfigManager* GetConfig() const { return m_config.get(); }
    DiagnosticContext* GetDiagnosticContext() const { return m_diagnosticContext.get(); }
    CapabilityRegistry* GetCapabilityRegistry() const { return m_capabilityRegistry.get(); }
    SprintCompatibilityLogger* GetSprintCompatibilityLogger() const { return m_sprintCompatibilityLogger.get(); }
    MatrixClassifier* GetMatrixClassifier() const { return m_matrixClassifier.get(); }
    TemporalCameraFilter* GetTemporalCameraFilter() const { return m_temporalCameraFilter.get(); }
    TemporalDepthFilter* GetTemporalDepthFilter() const { return m_temporalDepthFilter.get(); }
    CandidateCollector* GetCandidateCollector() const { return m_candidateCollector.get(); }
    DepthCandidateCollector* GetDepthCandidateCollector() const { return m_depthCandidateCollector.get(); }
    CameraLockManager* GetCameraLockManager() const { return m_cameraLockManager.get(); }
    DepthLockManager* GetDepthLockManager() const { return m_depthLockManager.get(); }

    SignatureScanner* GetSignatureScanner() const { return m_signatureScanner.get(); }
    PointerChainResolver* GetPointerChainResolver() const { return m_pointerChainResolver.get(); }
    PageScanner* GetPageScanner() const { return m_pageScanner.get(); }
    
    EngineDetector* GetEngineDetector() const { return m_engineDetector.get(); }
    engine_scanners::UniversalScanner* GetUniversalScanner() const { return m_universalScanner.get(); }
    FrameCoordinator* GetFrameCoordinator() const { return m_frameCoordinator.get(); }

private:
    SubsystemContext() = default;
    ~SubsystemContext() = default;

    std::shared_ptr<ILogger> m_logger;
    std::shared_ptr<ConfigManager> m_config;
    std::unique_ptr<DiagnosticContext> m_diagnosticContext;
    std::unique_ptr<CapabilityRegistry> m_capabilityRegistry;
    std::unique_ptr<SprintCompatibilityLogger> m_sprintCompatibilityLogger;
    std::unique_ptr<MatrixClassifier> m_matrixClassifier;
    std::unique_ptr<TemporalCameraFilter> m_temporalCameraFilter;
    std::unique_ptr<TemporalDepthFilter> m_temporalDepthFilter;
    std::unique_ptr<CandidateCollector> m_candidateCollector;
    std::unique_ptr<DepthCandidateCollector> m_depthCandidateCollector;
    std::unique_ptr<CameraLockManager> m_cameraLockManager;
    std::unique_ptr<DepthLockManager> m_depthLockManager;

    std::unique_ptr<SignatureScanner> m_signatureScanner;
    std::unique_ptr<PointerChainResolver> m_pointerChainResolver;
    std::unique_ptr<PageScanner> m_pageScanner;

    std::unique_ptr<EngineDetector> m_engineDetector;
    std::unique_ptr<engine_scanners::UniversalScanner> m_universalScanner;
    std::unique_ptr<FrameCoordinator> m_frameCoordinator;
};

} // namespace vrinject
