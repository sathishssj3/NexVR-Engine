#include "heuristics/reverse_z_detector.h"

namespace vrinject {

DepthConvention ReverseZDetector::Detect(const DepthCandidate& candidate, float lastClearDepth) {
    if (!candidate.valid) return DepthConvention::Unknown;

    // Standard depth clears to 1.0f (far plane)
    // Reverse-Z depth clears to 0.0f (far plane)
    if (lastClearDepth < 0.1f) {
        return DepthConvention::ReverseZ;
    } else if (lastClearDepth > 0.9f) {
        return DepthConvention::Standard;
    }
    
    return DepthConvention::Unknown;
}

} // namespace vrinject
