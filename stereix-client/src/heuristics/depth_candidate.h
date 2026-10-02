#pragma once
#include "core/depth_resource_identity.h"
#include <cstdint>

namespace vrinject {

struct DepthCandidate {
    DepthResourceIdentity identity;

    // Temporal tracking
    uint64_t frameSeen = 0;
    uint64_t lastFrameSeen = 0;
    
    // Frequencies
    uint32_t clearCount = 0;
    float lastClearDepth = 1.0f;
    uint32_t bindCount = 0;
    uint32_t updateFrequency = 0;
    
    // Scoring
    float lifetimeScore = 0.0f;
    float resolutionScore = 0.0f;
    float bindFrequencyScore = 0.0f;
    float clearPatternScore = 0.0f;
    float temporalStabilityScore = 0.0f;
    float classifierScore = 0.0f;
    
    float totalScore = 0.0f;
    float confidence = 0.0f;
    
    // Whether it passed initial classification
    bool valid = true;
};

} // namespace vrinject
