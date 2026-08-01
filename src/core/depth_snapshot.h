#pragma once
#include "depth_resource_identity.h"
#include "depth_convention.h"
#include <cstdint>

namespace vrinject {

struct DepthSnapshot {
    uint32_t deviceGeneration = 0;
    uint32_t swapchainGeneration = 0;
    uint32_t depthGeneration = 0;
    
    DepthResourceIdentity identity;
    DepthConvention convention = DepthConvention::Unknown;
    
    float confidence = 0.0f; // 0.0 to 100.0

    bool IsValid() const {
        return identity.nativeHandle != nullptr && confidence > 0.0f;
    }
};

} // namespace vrinject
