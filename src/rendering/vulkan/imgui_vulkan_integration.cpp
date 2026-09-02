#include "rendering/vulkan/imgui_vulkan_integration.h"
#include "core/logger.h"
#include "core/overlay_manager.h"
#include "rendering/vulkan/vulkan_dispatch_table.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "backends/imgui_impl_vulkan.h"

namespace vrinject {
namespace vulkan {

bool ImGuiVulkanIntegration::Initialize(
    VkInstance instance,
    VkPhysicalDevice physicalDevice,
    VkDevice device,
    uint32_t queueFamily,
    VkQueue queue,
    VkFormat colorFormat
) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized) return true;
    if (!instance || !physicalDevice || !device || !queue) return false;
    if (!ImGui::GetCurrentContext()) return false;

    m_device = device;
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(device);

    // Create Descriptor Pool for ImGui
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1000 * IM_ARRAYSIZE(poolSizes);
    poolInfo.poolSizeCount = static_cast<uint32_t>(IM_ARRAYSIZE(poolSizes));
    poolInfo.pPoolSizes = poolSizes;

    if (dt && dt->CreateDescriptorPool(device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
            LOG_ERROR("ImGuiVulkan: Failed to create descriptor pool");
            return false;
        }
    }

    // Create a simple RenderPass for ImGui rendering
    VkAttachmentDescription attachment = {};
    attachment.format = colorFormat;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachment = {};
    colorAttachment.attachment = 0;
    colorAttachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachment;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpInfo = {};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 1;
    rpInfo.pAttachments = &attachment;
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    rpInfo.dependencyCount = 1;
    rpInfo.pDependencies = &dependency;

    PFN_vkCreateRenderPass pfnCreateRenderPass = (PFN_vkCreateRenderPass)vkGetDeviceProcAddr(device, "vkCreateRenderPass");
    if (!pfnCreateRenderPass) pfnCreateRenderPass = vkCreateRenderPass;

    if (pfnCreateRenderPass(device, &rpInfo, nullptr, &m_renderPass) != VK_SUCCESS) {
        LOG_ERROR("ImGuiVulkan: Failed to create render pass");
        return false;
    }

    ImGui_ImplVulkan_InitInfo initInfo = {};
    initInfo.Instance = instance;
    initInfo.PhysicalDevice = physicalDevice;
    initInfo.Device = device;
    initInfo.QueueFamily = queueFamily;
    initInfo.Queue = queue;
    initInfo.PipelineCache = VK_NULL_HANDLE;
    initInfo.DescriptorPool = m_descriptorPool;
    initInfo.Subpass = 0;
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = 2;
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.RenderPass = m_renderPass;

    if (!ImGui_ImplVulkan_Init(&initInfo)) {
        LOG_ERROR("ImGuiVulkan: ImGui_ImplVulkan_Init failed!");
        return false;
    }

    m_initialized = true;
    LOG_INFO("ImGuiVulkan: Initialized successfully.");
    return true;
}

void ImGuiVulkanIntegration::Render(VkCommandBuffer cmdBuffer, VkFramebuffer framebuffer, VkExtent2D extent) {
    if (!m_initialized || !cmdBuffer || !framebuffer) return;

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplWin32_NewFrame();

    OverlayManager::GetInstance().Render();

    VkRenderPassBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.renderPass = m_renderPass;
    info.framebuffer = framebuffer;
    info.renderArea.extent = extent;
    info.clearValueCount = 0;
    info.pClearValues = nullptr;

    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(m_device);
    if (dt && dt->CmdBeginRenderPass) {
        dt->CmdBeginRenderPass(cmdBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuffer);
        dt->CmdEndRenderPass(cmdBuffer);
    } else {
        vkCmdBeginRenderPass(cmdBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuffer);
        vkCmdEndRenderPass(cmdBuffer);
    }
}

void ImGuiVulkanIntegration::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized) {
        ImGui_ImplVulkan_Shutdown();
        auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(m_device);
        if (m_renderPass != VK_NULL_HANDLE) {
            PFN_vkDestroyRenderPass pfnDestroyRenderPass = (PFN_vkDestroyRenderPass)vkGetDeviceProcAddr(m_device, "vkDestroyRenderPass");
            if (!pfnDestroyRenderPass) pfnDestroyRenderPass = vkDestroyRenderPass;
            pfnDestroyRenderPass(m_device, m_renderPass, nullptr);
            m_renderPass = VK_NULL_HANDLE;
        }
        if (m_descriptorPool != VK_NULL_HANDLE) {
            if (dt) dt->DestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
            else vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
            m_descriptorPool = VK_NULL_HANDLE;
        }
        m_initialized = false;
        LOG_INFO("ImGuiVulkan: Shutdown completed.");
    }
}

} // namespace vulkan
} // namespace vrinject
