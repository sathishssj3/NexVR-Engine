#pragma once
#include "render_frame_snapshot.h"
#include "camera_snapshot.h"
#include "frame_graph_profiler.h"
#include <string>

namespace vrinject {
struct FrameTimingSnapshot;
namespace vulkan {

class VulkanSnapshotValidator {
public:
    static bool Validate(const RenderFrameSnapshot& snapshot, std::string& outError);
    static bool ValidateCamera(const CameraSnapshot& snapshot, std::string& outError);

    struct DepthValidationResult {
        bool isValid = false;
        const char* reason = "Unknown";
    };

    static DepthValidationResult ValidateDepthSnapshot(const struct VulkanDepthSnapshot& snapshot);
    static bool ValidateFrameTiming(const FrameTimingSnapshot& snapshot, std::string& outError);
    static bool ValidatePerformanceSnapshot(const PerformanceSnapshot& snapshot, std::string& outError);

    struct StereoValidationState {
        bool hasPipeline = false;
        bool hasDescriptors = false;
        bool hasCommandBuffers = false;
        bool queueValid = false;
        bool layoutsValid = false;
        uint64_t resourceGeneration = 0;
        uint64_t expectedGeneration = 0;
    };

    static bool ValidateStereoState(const StereoValidationState& state, std::string& outError);
};

} // namespace vulkan
} // namespace vrinject
