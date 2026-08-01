#pragma once
#include <vulkan/vulkan.h>

namespace vrinject {
namespace vulkan {
namespace hooks {

void InstallVulkanHooks();
void RemoveVulkanHooks();

} // namespace hooks
} // namespace vulkan
} // namespace vrinject
