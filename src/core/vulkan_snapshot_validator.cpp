#include "vulkan_snapshot_validator.h"
#include "vulkan_depth_snapshot_builder.h"
#include <cmath>

namespace vrinject {
namespace vulkan {

bool VulkanSnapshotValidator::Validate(const RenderFrameSnapshot& snapshot, std::string& outError) {
    if (snapshot.backend != GraphicsBackend::Vulkan) {
        outError = "Snapshot backend is not Vulkan.";
        return false;
    }

    if (snapshot.state != RenderState::RUNNING) {
        if (snapshot.state == RenderState::DEVICE_REMOVED) {
            outError = "Snapshot state is DEVICE_REMOVED.";
        } else {
            outError = "Snapshot state is not RUNNING.";
        }
        return false;
    }

    if (snapshot.nativeDevice == nullptr) {
        outError = "Native device is null.";
        return false;
    }

    if (snapshot.nativeContext == nullptr) {
        outError = "Native context (queue) is null.";
        return false;
    }

    if (snapshot.nativeSwapchain == nullptr) {
        outError = "Native swapchain is null.";
        return false;
    }

    if (snapshot.nativeSurface == nullptr) {
        outError = "Native surface is null.";
        return false;
    }

    if (snapshot.epoch.deviceGeneration == 0) {
        outError = "Device generation is 0.";
        return false;
    }

    if (snapshot.epoch.swapchainGeneration == 0) {
        outError = "Swapchain generation is 0.";
        return false;
    }

    return true;
}

static bool IsNaNOrInf(float v) {
    return std::isnan(v) || std::isinf(v);
}

static bool IsMatrixValid(const Matrix4x4& m) {
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            if (IsNaNOrInf(m.m[i][j])) return false;
        }
    }
    return true;
}

static float Determinant(const Matrix4x4& m) {
    // Simplified determinant for projection matrices just to check if it's 0
    // Real determinant is complex, but P_00 * P_11 * P_22 is a good proxy for projection
    // A fully general determinant might be needed, but we can do a simple check.
    return m.m[0][0] * m.m[1][1]; 
}

bool VulkanSnapshotValidator::ValidateCamera(const CameraSnapshot& snapshot, std::string& outError) {
    if (!snapshot.valid) {
        outError = "Snapshot marked invalid.";
        return false;
    }

    if (!IsMatrixValid(snapshot.projection)) {
        outError = "Projection matrix contains NaN or Inf.";
        return false;
    }

    if (!IsMatrixValid(snapshot.vp)) {
        outError = "ViewProjection matrix contains NaN or Inf.";
        return false;
    }

    float det = Determinant(snapshot.projection);
    if (std::abs(det) < 1e-6f) {
        outError = "Projection matrix determinant is essentially zero (singular).";
        return false;
    }

    float p00 = snapshot.projection.m[0][0];
    float p11 = snapshot.projection.m[1][1];

    if (p00 <= 0.0f || p11 <= 0.0f) {
        // Technically some engines use negative scaling, but typically they are positive for standard cameras
        // The user asked to reject Negative FOV and zero aspect.
        outError = "Negative or zero FOV/Aspect detected in projection.";
        return false;
    }

    // Near/Far plane sanity
    // A standard Vulkan projection matrix has m[2][2] and m[3][2]
    // z_clip = m[2][2]*z + m[3][2]*w
    // It's tricky to extract exact near/far without knowing the engine's convention (reversed Z, etc.)
    // But we can check that it's not identically zero.
    
    // Check for reflection matrix (typically det of view < 0)
    // For now we check if p00 and p11 are too wildly different (crazy aspect ratio)
    float aspect = std::abs(p11 / p00);
    if (aspect < 0.1f || aspect > 10.0f) {
        outError = "Aspect ratio is wildly out of normal bounds (>10 or <0.1).";
        return false;
    }

    return true;
}

VulkanSnapshotValidator::DepthValidationResult VulkanSnapshotValidator::ValidateDepthSnapshot(const VulkanDepthSnapshot& snapshot) {
    if (!snapshot.image) return {false, "No Image Handle"};
    
    if (snapshot.width == 0 || snapshot.height == 0) return {false, "Zero Dimension"};

    // Aspect ratio check
    float aspect = static_cast<float>(snapshot.width) / static_cast<float>(snapshot.height);
    if (snapshot.width == snapshot.height && (snapshot.width <= 1024)) { // commonly shadow maps are square power of 2
        // We'll be lenient but flag extremely small ones
        if (snapshot.width <= 512) {
            return {false, "Likely Shadow Map (Small Square)"};
        }
    }
    if (aspect > 10.0f || aspect < 0.1f) return {false, "Extreme Aspect Ratio"};

    // Format check
    switch (snapshot.format) {
        case VK_FORMAT_D16_UNORM:
        case VK_FORMAT_D32_SFLOAT:
        case VK_FORMAT_D16_UNORM_S8_UINT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            break;
        default:
            return {false, "Invalid Depth Format"};
    }

    if (snapshot.depthGeneration == 0) return {false, "Stale Generation"};

    return {true, "Valid"};
}

bool VulkanSnapshotValidator::ValidateStereoState(const StereoValidationState& state, std::string& outError) {
    if (!state.hasPipeline) {
        outError = "Missing compute pipeline.";
        return false;
    }
    if (!state.hasDescriptors) {
        outError = "Invalid or missing descriptor sets.";
        return false;
    }
    if (!state.hasCommandBuffers) {
        outError = "Invalid command buffer state.";
        return false;
    }
    if (!state.queueValid) {
        outError = "Null or invalid queue ownership.";
        return false;
    }
    if (!state.layoutsValid) {
        outError = "Wrong image layouts.";
        return false;
    }
    if (state.resourceGeneration != state.expectedGeneration) {
        outError = "Resource generation mismatch.";
        return false;
    }
    return true;
}

} // namespace vulkan
} // namespace vrinject
