#include "compatibility_scorer.h"

#include "diagnostic_context.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace vrinject {
namespace {

float Clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

void CopyText(char* dest, size_t destSize, const char* src) {
    if (!dest || destSize == 0) return;
#ifdef _WIN32
    strncpy_s(dest, destSize, src ? src : "", _TRUNCATE);
#else
    std::strncpy(dest, src ? src : "", destSize - 1);
    dest[destSize - 1] = '\0';
#endif
}

} // namespace

CompatibilityScore CompatibilityScorer::Evaluate(const CameraSnapshot& camera,
                                                 const DepthSnapshot& depth,
                                                 GraphicsBackend backend,
                                                 const EngineDetection& engine) {
    CompatibilityScore result{};

    const bool cameraValid = camera.IsValid();
    const bool depthValid = depth.IsValid();

    const float cameraScore = cameraValid ? Clamp01(camera.confidence) : 0.0f;
    const float depthScore = depthValid ? Clamp01(depth.confidence / 100.0f) : 0.0f;
    const float backendScore = backend == GraphicsBackend::Unknown ? 0.0f : 1.0f;
    const float engineScore = engine.type == EngineType::Unknown
        ? 0.35f * Clamp01(engine.confidence)
        : Clamp01(engine.confidence);

    const float weighted =
        (cameraScore * 35.0f) +
        (depthScore * 35.0f) +
        (backendScore * 15.0f) +
        (engineScore * 15.0f);

    result.score = static_cast<uint32_t>(std::clamp(weighted, 0.0f, 100.0f) + 0.5f);

    if (!cameraValid) {
        AddReason(result, "camera", camera.confidence <= 0.5f
            ? "camera lock confidence is below stereo threshold"
            : "camera lock has no valid tracked graphics resource");
    }
    if (!depthValid) {
        AddReason(result, "depth", depth.confidence <= 0.0f
            ? "depth lock confidence is zero"
            : "depth lock has no valid native depth resource");
    } else if (depth.confidence < 50.0f) {
        AddReason(result, "depth", "depth lock confidence is weak");
    }
    if (backend == GraphicsBackend::Unknown) {
        AddReason(result, "api", "graphics API is unknown");
    }
    if (engine.type == EngineType::Unknown) {
        AddReason(result, "engine", "engine detection fell back to unknown/custom tuning");
    } else if (engine.confidence < 0.7f) {
        AddReason(result, "engine", "engine detection confidence is weak");
    }

    result.shouldAttemptStereo = cameraValid && depthValid && backend != GraphicsBackend::Unknown && result.score >= 70;
    if (result.shouldAttemptStereo) {
        result.readiness = CompatibilityReadiness::Ready;
    } else if (result.score >= 40) {
        result.readiness = CompatibilityReadiness::Degraded;
    } else {
        result.readiness = CompatibilityReadiness::Passthrough2D;
    }

    return result;
}

void CompatibilityScorer::PostDiagnostic(const CompatibilityScore& score) {
    if (score.readiness == CompatibilityReadiness::Ready) return;

    char message[128] = {};
    if (score.reasonCount > 0) {
        std::snprintf(message, sizeof(message), "score=%u signal=%s reason=%s",
                      score.score,
                      score.reasons[0].signal,
                      score.reasons[0].message);
    } else {
        std::snprintf(message, sizeof(message), "score=%u stereo disabled by compatibility gate", score.score);
    }

    DiagnosticContext::Get().PostEvent(
        score.readiness == CompatibilityReadiness::Degraded ? DiagnosticLevel::Warning : DiagnosticLevel::Error,
        "Compatibility",
        message);
}

void CompatibilityScorer::AddReason(CompatibilityScore& score, const char* signal, const char* message) {
    if (score.reasonCount >= score.reasons.size()) return;

    auto& reason = score.reasons[score.reasonCount++];
    CopyText(reason.signal, sizeof(reason.signal), signal);
    CopyText(reason.message, sizeof(reason.message), message);
}

} // namespace vrinject
