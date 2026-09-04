#pragma once
#include <vulkan/vulkan.h>

namespace vrinject {
namespace vulkan {
namespace hooks {

void InstallVulkanHooks();
void RemoveVulkanHooks();

/// @brief True once the Vulkan loader has loaded us as an implicit/explicit layer.
/// @details Set from VRInject_vkNegotiateLoaderLayerInterfaceVersion, which the loader
///          calls at layer-load time - long before RuntimeState's background thread
///          reaches HookManager::InitializeHooks(). Layer interception and MinHook
///          detours on vulkan-1.dll's exported symbols are mutually exclusive: with
///          both active the loader faults with 0xC0000409 during swapchain creation.
/// @thread_safety Thread-safe (atomic).
bool IsLayerActive();

/// @brief Record that the loader has activated us as a Vulkan layer.
/// @note Called only from the layer's negotiate entry point.
void SetLayerActive();

} // namespace hooks
} // namespace vulkan
} // namespace vrinject
