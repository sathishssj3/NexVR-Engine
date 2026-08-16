#include "rendering/dx12/dx12_resource_validator.h"

namespace vrinject {

Dx12ValidationResult Dx12ResourceValidator::Validate(const RenderFrameSnapshot& snapshot, const GraphicsResourceEpoch& currentEpoch) {
    Dx12ValidationResult result;
    result.expectedEpoch = currentEpoch;
    result.actualEpoch = snapshot.epoch;
    result.recoverable = true;

    if (snapshot.epoch.deviceGeneration != currentEpoch.deviceGeneration ||
        snapshot.epoch.swapchainGeneration != currentEpoch.swapchainGeneration ||
        snapshot.epoch.resourceGeneration != currentEpoch.resourceGeneration) {
        result.status = Dx12ValidationStatus::INVALID_GENERATION;
        result.recoverable = true; // Typically recoverable by awaiting the next frame
        return result;
    }

    if (!snapshot.nativeDevice) {
        result.status = Dx12ValidationStatus::NULL_DEVICE;
        result.recoverable = false;
        return result;
    }

    if (!snapshot.nativeContext) {
        result.status = Dx12ValidationStatus::NULL_QUEUE;
        result.recoverable = false;
        return result;
    }

    if (!snapshot.nativeSwapchain) {
        result.status = Dx12ValidationStatus::NULL_SWAPCHAIN;
        result.recoverable = false;
        return result;
    }

    if (!snapshot.backBuffer) {
        result.status = Dx12ValidationStatus::INVALID_BACKBUFFER;
        result.recoverable = true; // Wait for rebuild
        return result;
    }

    if (snapshot.width == 0 || snapshot.height == 0) {
        result.status = Dx12ValidationStatus::INVALID_DIMENSIONS;
        result.recoverable = true; // Wait for rebuild
        return result;
    }

    if (IsDeviceLost(static_cast<ID3D12Device*>(snapshot.nativeDevice))) {
        result.status = Dx12ValidationStatus::DEVICE_REMOVED;
        result.recoverable = true; // Game might recreate device
        return result;
    }

    result.status = Dx12ValidationStatus::VALID;
    return result;
}

bool Dx12ResourceValidator::IsDeviceLost(ID3D12Device* device) {
    if (!device) return true;
    HRESULT reason = device->GetDeviceRemovedReason();
    return FAILED(reason);
}

} // namespace vrinject
