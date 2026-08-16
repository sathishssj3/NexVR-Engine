#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>

namespace vrinject {
namespace vulkan {

struct ResolvedBufferInfo {
    VkBuffer buffer = nullptr;
    VkDeviceMemory memory = nullptr;
    VkDeviceSize offset = 0;
    VkDeviceSize size = 0;
    void* mappedPointer = nullptr;
    uint64_t generation = 0;
    
    bool IsValid() const {
        return buffer != nullptr && mappedPointer != nullptr;
    }
};

class VulkanBufferResolver {
public:
    static VulkanBufferResolver& Get() {
        static VulkanBufferResolver instance;
        return instance;
    }

    // Resolves a uniform/storage buffer given a descriptor set and binding index.
    // If the descriptor is a dynamic buffer type, dynamicOffset is added to the base offset.
    ResolvedBufferInfo ResolveBuffer(VkDevice device, VkDescriptorSet set, uint32_t binding, uint32_t dynamicOffset = 0) const;

private:
    VulkanBufferResolver() = default;
};

} // namespace vulkan
} // namespace vrinject
