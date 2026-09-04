#pragma once

#include "heuristics/camera_snapshot.h"
#include "heuristics/depth_snapshot.h"
#include "core/engine_detector.h"
#include "core/graphics_types.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace vrinject {

enum class CompatibilityReadiness {
    Ready,
    Degraded,
    Passthrough2D
};

struct CompatibilityReason {
    char signal[24] = {};
    char message[96] = {};
};

struct CompatibilityScore {
    uint32_t score = 0;
    CompatibilityReadiness readiness = CompatibilityReadiness::Passthrough2D;
    bool shouldAttemptStereo = false;
    std::array<CompatibilityReason, 8> reasons{};
    size_t reasonCount = 0;
};

class CompatibilityScorer {
public:
    static CompatibilityScore Evaluate(const CameraSnapshot& camera,
                                       const DepthSnapshot& depth,
                                       GraphicsBackend backend,
                                       const EngineDetection& engine);

    static void PostDiagnostic(const CompatibilityScore& score);

private:
    static void AddReason(CompatibilityScore& score, const char* signal, const char* message);
};

} // namespace vrinject
