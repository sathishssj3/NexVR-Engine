#include "dx11_resource_validator.h"

namespace vrinject {

Dx11ValidationResult Dx11ResourceValidator::Validate(const RenderFrameSnapshot& snapshot, const GraphicsResourceEpoch& currentEpoch) {
    Dx11ValidationResult result;
    result.expectedEpoch = currentEpoch;
    result.actualEpoch = snapshot.epoch;
    result.recoverable = true; // Most failures are recoverable via lifecycle reinitialization

    if (snapshot.epoch != currentEpoch) {
        result.status = Dx11ValidationStatus::STALE_SNAPSHOT;
        result.recoverable = false; // The snapshot itself is outdated for the current frame
        return result;
    }

    if (!snapshot.nativeDevice) {
        result.status = Dx11ValidationStatus::NULL_DEVICE;
        return result;
    }

    if (!snapshot.nativeContext) {
        result.status = Dx11ValidationStatus::NULL_CONTEXT;
        return result;
    }

    if (!snapshot.nativeSwapchain) {
        result.status = Dx11ValidationStatus::NULL_SWAPCHAIN;
        return result;
    }

    if (!snapshot.backBuffer) {
        result.status = Dx11ValidationStatus::INVALID_BACKBUFFER;
        return result;
    }

    if (snapshot.width == 0 || snapshot.height == 0) {
        result.status = Dx11ValidationStatus::INVALID_DIMENSIONS;
        return result;
    }

    if (snapshot.format == DXGI_FORMAT_UNKNOWN) {
        result.status = Dx11ValidationStatus::INVALID_FORMAT;
        return result;
    }

    if (IsDeviceLost(static_cast<ID3D11Device*>(snapshot.nativeDevice))) {
        HRESULT reason = static_cast<ID3D11Device*>(snapshot.nativeDevice)->GetDeviceRemovedReason();
        if (reason == DXGI_ERROR_DEVICE_RESET) {
            result.status = Dx11ValidationStatus::DEVICE_RESET;
        } else {
            result.status = Dx11ValidationStatus::DEVICE_REMOVED;
        }
        return result;
    }

    result.status = Dx11ValidationStatus::VALID;
    return result;
}

bool Dx11ResourceValidator::IsDeviceLost(ID3D11Device* device) {
    if (!device) return true;
    HRESULT reason = device->GetDeviceRemovedReason();
    return FAILED(reason);
}

} // namespace vrinject
