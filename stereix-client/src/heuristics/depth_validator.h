#pragma once
#include "heuristics/depth_candidate.h"

namespace vrinject {

class DepthValidator {
public:
    // Analyzes a candidate and rejects it (valid = false) if it matches known false-positive heuristics
    // e.g., reflection buffers, shadow cascades, UI depth, low resolution
    static void Validate(DepthCandidate& candidate, uint32_t swapchainWidth, uint32_t swapchainHeight);
};

} // namespace vrinject
