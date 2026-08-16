#pragma once

#include <vulkan/vulkan.h>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace vrinject {
namespace vulkan {

struct RenderPassRecord {
    VkCommandBuffer commandBuffer = nullptr;
    VkDevice device = nullptr;
    VkRenderPass renderPass = nullptr; // Null if dynamic rendering
    VkFramebuffer framebuffer = nullptr; // Null if dynamic rendering
    
    // Direct attachments from either the framebuffer or dynamic rendering
    VkImageView depthAttachment = nullptr;
    std::vector<VkImageView> colorAttachments;
    
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t layers = 1;
    bool dynamicRendering = false;
};

class VulkanRenderPassTracker {
public:
    static VulkanRenderPassTracker& Get() {
        static VulkanRenderPassTracker instance;
        return instance;
    }

    void OnCmdBeginRenderPass(VkDevice device, VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin);
    void OnCmdEndRenderPass(VkDevice device, VkCommandBuffer commandBuffer);

    void OnCmdBeginRendering(VkDevice device, VkCommandBuffer commandBuffer, const VkRenderingInfo* pRenderingInfo);
    void OnCmdEndRendering(VkDevice device, VkCommandBuffer commandBuffer);

    // Returns a copy of the active render passes for candidate collection
    std::vector<RenderPassRecord> GetActiveRenderPasses() const;

private:
    VulkanRenderPassTracker() = default;

    mutable std::shared_mutex m_mutex;
    // Map from command buffer to active render pass
    std::unordered_map<VkCommandBuffer, RenderPassRecord> m_activePasses;
};

} // namespace vulkan
} // namespace vrinject
