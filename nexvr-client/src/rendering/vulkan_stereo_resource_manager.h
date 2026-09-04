#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>
#include <mutex>

namespace vrinject {
namespace vulkan {

class VulkanStereoResourceManager {
public:
    VulkanStereoResourceManager(VkDevice device, VkPhysicalDevice physicalDevice);
    ~VulkanStereoResourceManager();

    bool Resize(uint32_t width, uint32_t height);
    void Destroy();
    bool Recreate(); // Force recreation of resources

    VkImageView GetLeftEyeView() const { return m_leftEyeView; }
    VkImageView GetRightEyeView() const { return m_rightEyeView; }
    VkImage GetLeftEyeImage() const { return m_leftEyeImage; }
    VkImage GetRightEyeImage() const { return m_rightEyeImage; }
    VkSampler GetSampler() const { return m_sampler; }

    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }
    uint64_t GetGeneration() const { return m_generation; }

private:
    bool AllocateResources();
    void FreeResources();
    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    std::mutex m_mutex;

    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint64_t m_generation = 0;

    VkImage m_leftEyeImage = VK_NULL_HANDLE;
    VkImage m_rightEyeImage = VK_NULL_HANDLE;
    VkDeviceMemory m_leftEyeMemory = VK_NULL_HANDLE;
    VkDeviceMemory m_rightEyeMemory = VK_NULL_HANDLE;
    
    VkImageView m_leftEyeView = VK_NULL_HANDLE;
    VkImageView m_rightEyeView = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
};

} // namespace vulkan
} // namespace vrinject
