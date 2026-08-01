#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace vrinject {
namespace vulkan {

struct VulkanAsyncSchedulerConfig {
    uint32_t graphicsQueueFamily = 0;
    uint32_t selectedQueueFamily = 0;
    bool asyncComputeAvailable = false;
};

class VulkanAsyncScheduler {
public:
    static VulkanAsyncSchedulerConfig SelectQueueFamily(
        const std::vector<VkQueueFamilyProperties>& families,
        uint32_t graphicsQueueFamily);
};

} // namespace vulkan
} // namespace vrinject
