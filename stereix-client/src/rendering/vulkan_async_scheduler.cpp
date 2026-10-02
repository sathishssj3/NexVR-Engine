#include "rendering/vulkan_async_scheduler.h"

namespace vrinject {
namespace vulkan {

VulkanAsyncSchedulerConfig VulkanAsyncScheduler::SelectQueueFamily(
    const std::vector<VkQueueFamilyProperties>& families,
    uint32_t graphicsQueueFamily) {
    VulkanAsyncSchedulerConfig config{};
    config.graphicsQueueFamily = graphicsQueueFamily;
    config.selectedQueueFamily = graphicsQueueFamily;

    for (uint32_t i = 0; i < families.size(); ++i) {
        const VkQueueFlags flags = families[i].queueFlags;
        const bool compute = (flags & VK_QUEUE_COMPUTE_BIT) != 0;
        const bool graphics = (flags & VK_QUEUE_GRAPHICS_BIT) != 0;
        if (compute && !graphics) {
            config.selectedQueueFamily = i;
            config.asyncComputeAvailable = true;
            return config;
        }
    }
    return config;
}

} // namespace vulkan
} // namespace vrinject
