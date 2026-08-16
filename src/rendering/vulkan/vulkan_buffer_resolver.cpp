#include "rendering/vulkan/vulkan_buffer_resolver.h"
#include "rendering/vulkan/vulkan_descriptor_tracker.h"
#include "rendering/vulkan/vulkan_resource_tracker.h"

namespace vrinject {
namespace vulkan {

ResolvedBufferInfo VulkanBufferResolver::ResolveBuffer(VkDevice device, VkDescriptorSet set, uint32_t binding, uint32_t dynamicOffset) const {
    ResolvedBufferInfo result = {};

    auto& descriptorTracker = VulkanDescriptorTracker::Get();
    auto& resourceTracker = VulkanResourceTracker::Get();

    VulkanDescriptorSetInfo setInfo;
    if (!descriptorTracker.GetDescriptorSetInfo(device, set, setInfo)) {
        return result; // Set not found
    }

    auto it = setInfo.bindings.find(binding);
    if (it == setInfo.bindings.end()) {
        return result; // Binding not found
    }

    const auto& descBinding = it->second;
    
    // Check if it's a buffer descriptor
    if (!descBinding.buffer || (
        descBinding.descriptorType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER &&
        descBinding.descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER &&
        descBinding.descriptorType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC &&
        descBinding.descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)) 
    {
        return result;
    }

    VulkanBufferInfo bufferInfo;
    if (!resourceTracker.GetBufferInfo(device, descBinding.buffer, bufferInfo)) {
        return result; // Buffer not tracked
    }

    result.buffer = bufferInfo.buffer;
    result.generation = bufferInfo.generation;

    // Calculate effective offset
    VkDeviceSize effectiveOffset = descBinding.offset;
    if (descBinding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC || 
        descBinding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC) 
    {
        effectiveOffset += dynamicOffset;
    }
    
    result.offset = effectiveOffset;
    result.size = descBinding.range;
    
    result.memory = bufferInfo.boundMemory;
    VkDeviceSize absoluteMemoryOffset = bufferInfo.memoryOffset + result.offset;
    
    // Find mapped memory for this buffer
    auto mappedMemories = resourceTracker.GetAllMappedMemory();
    for (const auto& mapped : mappedMemories) {
        if (mapped.memory == result.memory && mapped.device == device) {
            // Check if absoluteMemoryOffset falls within mapped region
            if (absoluteMemoryOffset >= mapped.offset && absoluteMemoryOffset < mapped.offset + mapped.size) {
                result.mappedPointer = reinterpret_cast<uint8_t*>(mapped.hostPointer) + (absoluteMemoryOffset - mapped.offset);
                break;
            }
        }
    }
    
    return result;
}

} // namespace vulkan
} // namespace vrinject
