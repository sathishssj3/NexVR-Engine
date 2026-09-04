#pragma once
#include "rendering/dx11/dx11_lifecycle_manager.h"

namespace vrinject {

enum class Dx11ValidationStatus {
    VALID,
    NULL_DEVICE,
    NULL_CONTEXT,
    NULL_SWAPCHAIN,
    INVALID_GENERATION,
    DEVICE_REMOVED,
    DEVICE_RESET,
    INVALID_BACKBUFFER,
    INVALID_RTV,
    INVALID_DIMENSIONS,
    INVALID_FORMAT,
    STALE_SNAPSHOT
};

struct Dx11ValidationResult {
    Dx11ValidationStatus status;
    GraphicsResourceEpoch expectedEpoch;
    GraphicsResourceEpoch actualEpoch;
    bool recoverable;
};

class Dx11ResourceValidator {
public:
    // Validates that a snapshot is internally consistent and matches the expected epoch
    static Dx11ValidationResult Validate(const RenderFrameSnapshot& snapshot, const GraphicsResourceEpoch& currentEpoch);
    
    // Checks physical device state to see if it's lost
    static bool IsDeviceLost(ID3D11Device* device);
};

} // namespace vrinject
