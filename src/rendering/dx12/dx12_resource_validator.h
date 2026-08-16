#pragma once
#include "rendering/dx12/dx12_lifecycle_manager.h"

namespace vrinject {

enum class Dx12ValidationStatus {
    VALID,
    NULL_DEVICE,
    NULL_QUEUE,
    NULL_SWAPCHAIN,
    INVALID_GENERATION,
    DEVICE_REMOVED,
    DEVICE_RESET,
    INVALID_BACKBUFFER,
    INVALID_DIMENSIONS,
    INVALID_FORMAT,
    STALE_SNAPSHOT
};

struct Dx12ValidationResult {
    Dx12ValidationStatus status;
    GraphicsResourceEpoch expectedEpoch;
    GraphicsResourceEpoch actualEpoch;
    bool recoverable;
};

class Dx12ResourceValidator {
public:
    // Validates that a snapshot is internally consistent and matches the expected epoch
    static Dx12ValidationResult Validate(const RenderFrameSnapshot& snapshot, const GraphicsResourceEpoch& currentEpoch);
    
    // Checks physical device state to see if it's lost
    static bool IsDeviceLost(ID3D12Device* device);
};

} // namespace vrinject
