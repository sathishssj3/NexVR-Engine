#include "rendering/vulkan_stereo_resource_manager.h"
#include "rendering/vulkan/vulkan_dispatch_table.h"
#include <stdexcept>

namespace vrinject {
namespace vulkan {

VulkanStereoResourceManager::VulkanStereoResourceManager(VkDevice device, VkPhysicalDevice physicalDevice)
    : m_device(device), m_physicalDevice(physicalDevice) {
}

VulkanStereoResourceManager::~VulkanStereoResourceManager() {
    Destroy();
}

bool VulkanStereoResourceManager::Resize(uint32_t width, uint32_t height) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_width == width && m_height == height && m_leftEyeImage != VK_NULL_HANDLE) {
        return true; // No change needed
    }

    m_width = width;
    m_height = height;
    m_generation++;

    FreeResources();
    if (width > 0 && height > 0) {
        return AllocateResources();
    }
    return true;
}

bool VulkanStereoResourceManager::Recreate() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_generation++;
    FreeResources();
    if (m_width > 0 && m_height > 0) {
        return AllocateResources();
    }
    return true;
}

void VulkanStereoResourceManager::Destroy() {
    std::lock_guard<std::mutex> lock(m_mutex);
    FreeResources();
}

bool VulkanStereoResourceManager::AllocateResources() {
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(m_device);
    if (!dt) return false;

    // We assume RGBA8 UNORM as a safe output format
    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = m_width;
    imageInfo.extent.height = m_height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    // Create Left Eye Image
    if (dt->CreateImage(m_device, &imageInfo, nullptr, &m_leftEyeImage) != VK_SUCCESS) return false;
    // Create Right Eye Image
    if (dt->CreateImage(m_device, &imageInfo, nullptr, &m_rightEyeImage) != VK_SUCCESS) return false;

    VkMemoryRequirements memRequirements;
    dt->GetImageMemoryRequirements(m_device, m_leftEyeImage, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // Allocate & Bind Left Eye
    if (dt->AllocateMemory(m_device, &allocInfo, nullptr, &m_leftEyeMemory) != VK_SUCCESS) return false;
    dt->BindImageMemory(m_device, m_leftEyeImage, m_leftEyeMemory, 0);

    // Allocate & Bind Right Eye
    dt->GetImageMemoryRequirements(m_device, m_rightEyeImage, &memRequirements);
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    if (dt->AllocateMemory(m_device, &allocInfo, nullptr, &m_rightEyeMemory) != VK_SUCCESS) return false;
    dt->BindImageMemory(m_device, m_rightEyeImage, m_rightEyeMemory, 0);

    // Create Image Views
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    viewInfo.image = m_leftEyeImage;
    if (dt->CreateImageView(m_device, &viewInfo, nullptr, &m_leftEyeView) != VK_SUCCESS) return false;

    viewInfo.image = m_rightEyeImage;
    if (dt->CreateImageView(m_device, &viewInfo, nullptr, &m_rightEyeView) != VK_SUCCESS) return false;

    // Create Sampler (if not already created)
    if (m_sampler == VK_NULL_HANDLE) {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = 1.0f;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        
        if (dt->CreateSampler(m_device, &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS) return false;
    }

    return true;
}

void VulkanStereoResourceManager::FreeResources() {
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(m_device);
    if (!dt) return;

    if (m_leftEyeView) { dt->DestroyImageView(m_device, m_leftEyeView, nullptr); m_leftEyeView = VK_NULL_HANDLE; }
    if (m_rightEyeView) { dt->DestroyImageView(m_device, m_rightEyeView, nullptr); m_rightEyeView = VK_NULL_HANDLE; }
    
    if (m_leftEyeImage) { dt->DestroyImage(m_device, m_leftEyeImage, nullptr); m_leftEyeImage = VK_NULL_HANDLE; }
    if (m_rightEyeImage) { dt->DestroyImage(m_device, m_rightEyeImage, nullptr); m_rightEyeImage = VK_NULL_HANDLE; }
    
    if (m_leftEyeMemory) { dt->FreeMemory(m_device, m_leftEyeMemory, nullptr); m_leftEyeMemory = VK_NULL_HANDLE; }
    if (m_rightEyeMemory) { dt->FreeMemory(m_device, m_rightEyeMemory, nullptr); m_rightEyeMemory = VK_NULL_HANDLE; }

    if (m_sampler) { dt->DestroySampler(m_device, m_sampler, nullptr); m_sampler = VK_NULL_HANDLE; }
}

uint32_t VulkanStereoResourceManager::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    auto instanceDt = VulkanDispatchTable::Get().GetInstanceDispatch(VulkanDispatchTable::Get().GetInstanceForDevice(m_device));
    if (!instanceDt) return 0;

    VkPhysicalDeviceMemoryProperties memProperties;
    instanceDt->GetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return 0; // Fallback
}

} // namespace vulkan
} // namespace vrinject
