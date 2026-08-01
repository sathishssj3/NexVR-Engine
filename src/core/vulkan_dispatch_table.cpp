#include "vulkan_dispatch_table.h"
#include <cstring>
#include <iostream>

// Forward declarations for hooked functions (to be implemented in vulkan_hook.cpp)
namespace vrinject {
namespace vulkan {
namespace hooks {
    VKAPI_ATTR VkResult VKAPI_CALL Hooked_vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance);
    VKAPI_ATTR void VKAPI_CALL Hooked_vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks* pAllocator);
    VKAPI_ATTR VkResult VKAPI_CALL Hooked_vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice);
    VKAPI_ATTR void VKAPI_CALL Hooked_vkDestroyDevice(VkDevice device, const VkAllocationCallbacks* pAllocator);
    VKAPI_ATTR void VKAPI_CALL Hooked_vkGetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue* pQueue);
    VKAPI_ATTR VkResult VKAPI_CALL Hooked_vkCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain);
    VKAPI_ATTR void VKAPI_CALL Hooked_vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks* pAllocator);
    VKAPI_ATTR VkResult VKAPI_CALL Hooked_vkAcquireNextImageKHR(VkDevice device, VkSwapchainKHR swapchain, uint64_t timeout, VkSemaphore semaphore, VkFence fence, uint32_t* pImageIndex);
    VKAPI_ATTR VkResult VKAPI_CALL Hooked_vkQueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence);
    VKAPI_ATTR VkResult VKAPI_CALL Hooked_vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo);
    VKAPI_ATTR void VKAPI_CALL Hooked_vkDestroySurfaceKHR(VkInstance instance, VkSurfaceKHR surface, const VkAllocationCallbacks* pAllocator);

    // Sprint 5.2 Hook declarations
    VKAPI_ATTR VkResult VKAPI_CALL Hooked_vkCreateImage(VkDevice device, const VkImageCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImage* pImage);
    VKAPI_ATTR void VKAPI_CALL Hooked_vkDestroyImage(VkDevice device, VkImage image, const VkAllocationCallbacks* pAllocator);
    VKAPI_ATTR VkResult VKAPI_CALL Hooked_vkCreateBuffer(VkDevice device, const VkBufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBuffer* pBuffer);
    VKAPI_ATTR void VKAPI_CALL Hooked_vkDestroyBuffer(VkDevice device, VkBuffer buffer, const VkAllocationCallbacks* pAllocator);
    VKAPI_ATTR VkResult VKAPI_CALL Hooked_vkBindBufferMemory(VkDevice device, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize memoryOffset);
    VKAPI_ATTR VkResult VKAPI_CALL Hooked_vkMapMemory(VkDevice device, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void** ppData);
    VKAPI_ATTR void VKAPI_CALL Hooked_vkUnmapMemory(VkDevice device, VkDeviceMemory memory);
    VKAPI_ATTR VkResult VKAPI_CALL Hooked_vkAllocateDescriptorSets(VkDevice device, const VkDescriptorSetAllocateInfo* pAllocateInfo, VkDescriptorSet* pDescriptorSets);
    VKAPI_ATTR VkResult VKAPI_CALL Hooked_vkFreeDescriptorSets(VkDevice device, VkDescriptorPool descriptorPool, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets);
    VKAPI_ATTR void VKAPI_CALL Hooked_vkUpdateDescriptorSets(VkDevice device, uint32_t descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites, uint32_t descriptorCopyCount, const VkCopyDescriptorSet* pDescriptorCopies);
    VKAPI_ATTR VkResult VKAPI_CALL Hooked_vkResetDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool, VkDescriptorPoolResetFlags flags);
    VKAPI_ATTR void VKAPI_CALL Hooked_vkCmdBindDescriptorSets(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount, const uint32_t* pDynamicOffsets);
    VKAPI_ATTR void VKAPI_CALL Hooked_vkCmdExecuteCommands(VkCommandBuffer commandBuffer, uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers);

    VKAPI_ATTR void VKAPI_CALL Hooked_vkCmdBeginRenderPass(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, VkSubpassContents contents);
    VKAPI_ATTR void VKAPI_CALL Hooked_vkCmdEndRenderPass(VkCommandBuffer commandBuffer);
    VKAPI_ATTR void VKAPI_CALL Hooked_vkCmdBeginRendering(VkCommandBuffer commandBuffer, const VkRenderingInfo* pRenderingInfo);
    VKAPI_ATTR void VKAPI_CALL Hooked_vkCmdEndRendering(VkCommandBuffer commandBuffer);
    VKAPI_ATTR void VKAPI_CALL Hooked_vkCmdPipelineBarrier(VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers);
    VKAPI_ATTR void VKAPI_CALL Hooked_vkCmdPipelineBarrier2(VkCommandBuffer commandBuffer, const VkDependencyInfo* pDependencyInfo);
    VKAPI_ATTR void VKAPI_CALL Hooked_vkCmdWaitEvents(VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent* pEvents, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers);
    VKAPI_ATTR void VKAPI_CALL Hooked_vkCmdNextSubpass(VkCommandBuffer commandBuffer, VkSubpassContents contents);
    VKAPI_ATTR VkResult VKAPI_CALL Hooked_vkCreateImageView(VkDevice device, const VkImageViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImageView* pView);
    VKAPI_ATTR void VKAPI_CALL Hooked_vkDestroyImageView(VkDevice device, VkImageView imageView, const VkAllocationCallbacks* pAllocator);
    VKAPI_ATTR VkResult VKAPI_CALL Hooked_vkCreateFramebuffer(VkDevice device, const VkFramebufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkFramebuffer* pFramebuffer);
    VKAPI_ATTR void VKAPI_CALL Hooked_vkDestroyFramebuffer(VkDevice device, VkFramebuffer framebuffer, const VkAllocationCallbacks* pAllocator);
    VKAPI_ATTR void VKAPI_CALL Hooked_vkCmdClearDepthStencilImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearDepthStencilValue* pDepthStencil, uint32_t rangeCount, const VkImageSubresourceRange* pRanges);
}
}
}

namespace vrinject {
namespace vulkan {

VulkanDispatchTable& VulkanDispatchTable::Get() {
    static VulkanDispatchTable instance;
    return instance;
}

void VulkanDispatchTable::InitOriginalGetInstanceProcAddr(PFN_vkGetInstanceProcAddr original) {
    std::unique_lock lock(m_mutex);
    if (!m_originalGetInstanceProcAddr) {
        m_originalGetInstanceProcAddr = original;
    }
}

void VulkanDispatchTable::RegisterInstance(VkInstance instance) {
    if (!instance || !m_originalGetInstanceProcAddr) return;
    
    InstanceDispatchTable table;
    table.DestroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(m_originalGetInstanceProcAddr(instance, "vkDestroyInstance"));
    table.GetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(m_originalGetInstanceProcAddr(instance, "vkGetDeviceProcAddr"));
    table.CreateDevice = reinterpret_cast<PFN_vkCreateDevice>(m_originalGetInstanceProcAddr(instance, "vkCreateDevice"));
    table.DestroySurfaceKHR = reinterpret_cast<PFN_vkDestroySurfaceKHR>(m_originalGetInstanceProcAddr(instance, "vkDestroySurfaceKHR"));
    table.GetPhysicalDeviceProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(m_originalGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties"));
    table.GetPhysicalDeviceMemoryProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(m_originalGetInstanceProcAddr(instance, "vkGetPhysicalDeviceMemoryProperties"));

    std::unique_lock lock(m_mutex);
    m_instanceTables[instance] = table;
}

void VulkanDispatchTable::UnregisterInstance(VkInstance instance) {
    std::unique_lock lock(m_mutex);
    m_instanceTables.erase(instance);
}

void VulkanDispatchTable::RegisterDevice(VkDevice device, VkInstance instance) {
    if (!device || !instance || !m_originalGetInstanceProcAddr) return;

    PFN_vkGetDeviceProcAddr getDeviceProcAddr = nullptr;
    {
        std::shared_lock lock(m_mutex);
        auto it = m_instanceTables.find(instance);
        if (it != m_instanceTables.end()) {
            getDeviceProcAddr = it->second.GetDeviceProcAddr;
        }
    }

    if (!getDeviceProcAddr) {
        getDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(m_originalGetInstanceProcAddr(instance, "vkGetDeviceProcAddr"));
    }

    if (!getDeviceProcAddr) return;

    DeviceDispatchTable table;
    table.DestroyDevice = reinterpret_cast<PFN_vkDestroyDevice>(getDeviceProcAddr(device, "vkDestroyDevice"));
    table.GetDeviceQueue = reinterpret_cast<PFN_vkGetDeviceQueue>(getDeviceProcAddr(device, "vkGetDeviceQueue"));
    table.CreateSwapchainKHR = reinterpret_cast<PFN_vkCreateSwapchainKHR>(getDeviceProcAddr(device, "vkCreateSwapchainKHR"));
    table.DestroySwapchainKHR = reinterpret_cast<PFN_vkDestroySwapchainKHR>(getDeviceProcAddr(device, "vkDestroySwapchainKHR"));
    table.AcquireNextImageKHR = reinterpret_cast<PFN_vkAcquireNextImageKHR>(getDeviceProcAddr(device, "vkAcquireNextImageKHR"));
    table.QueueSubmit = reinterpret_cast<PFN_vkQueueSubmit>(getDeviceProcAddr(device, "vkQueueSubmit"));
    table.QueuePresentKHR = reinterpret_cast<PFN_vkQueuePresentKHR>(getDeviceProcAddr(device, "vkQueuePresentKHR"));
    table.DeviceWaitIdle = reinterpret_cast<PFN_vkDeviceWaitIdle>(getDeviceProcAddr(device, "vkDeviceWaitIdle"));
    table.QueueWaitIdle = reinterpret_cast<PFN_vkQueueWaitIdle>(getDeviceProcAddr(device, "vkQueueWaitIdle"));

    // Sprint 5.2 Pointers
    table.CreateImage = reinterpret_cast<PFN_vkCreateImage>(getDeviceProcAddr(device, "vkCreateImage"));
    table.DestroyImage = reinterpret_cast<PFN_vkDestroyImage>(getDeviceProcAddr(device, "vkDestroyImage"));
    table.CreateBuffer = reinterpret_cast<PFN_vkCreateBuffer>(getDeviceProcAddr(device, "vkCreateBuffer"));
    table.DestroyBuffer = reinterpret_cast<PFN_vkDestroyBuffer>(getDeviceProcAddr(device, "vkDestroyBuffer"));
    table.BindBufferMemory = reinterpret_cast<PFN_vkBindBufferMemory>(getDeviceProcAddr(device, "vkBindBufferMemory"));
    table.MapMemory = reinterpret_cast<PFN_vkMapMemory>(getDeviceProcAddr(device, "vkMapMemory"));
    table.UnmapMemory = reinterpret_cast<PFN_vkUnmapMemory>(getDeviceProcAddr(device, "vkUnmapMemory"));
    table.AllocateDescriptorSets = reinterpret_cast<PFN_vkAllocateDescriptorSets>(getDeviceProcAddr(device, "vkAllocateDescriptorSets"));
    table.FreeDescriptorSets = reinterpret_cast<PFN_vkFreeDescriptorSets>(getDeviceProcAddr(device, "vkFreeDescriptorSets"));
    table.UpdateDescriptorSets = reinterpret_cast<PFN_vkUpdateDescriptorSets>(getDeviceProcAddr(device, "vkUpdateDescriptorSets"));
    table.ResetDescriptorPool = reinterpret_cast<PFN_vkResetDescriptorPool>(getDeviceProcAddr(device, "vkResetDescriptorPool"));
    table.CmdBindDescriptorSets = reinterpret_cast<PFN_vkCmdBindDescriptorSets>(getDeviceProcAddr(device, "vkCmdBindDescriptorSets"));
    table.CmdExecuteCommands = reinterpret_cast<PFN_vkCmdExecuteCommands>(getDeviceProcAddr(device, "vkCmdExecuteCommands"));

    table.CmdBeginRenderPass = reinterpret_cast<PFN_vkCmdBeginRenderPass>(getDeviceProcAddr(device, "vkCmdBeginRenderPass"));
    table.CmdEndRenderPass = reinterpret_cast<PFN_vkCmdEndRenderPass>(getDeviceProcAddr(device, "vkCmdEndRenderPass"));
    table.CmdBeginRendering = reinterpret_cast<PFN_vkCmdBeginRendering>(getDeviceProcAddr(device, "vkCmdBeginRendering"));
    if (!table.CmdBeginRendering) table.CmdBeginRendering = reinterpret_cast<PFN_vkCmdBeginRendering>(getDeviceProcAddr(device, "vkCmdBeginRenderingKHR"));
    table.CmdEndRendering = reinterpret_cast<PFN_vkCmdEndRendering>(getDeviceProcAddr(device, "vkCmdEndRendering"));
    if (!table.CmdEndRendering) table.CmdEndRendering = reinterpret_cast<PFN_vkCmdEndRendering>(getDeviceProcAddr(device, "vkCmdEndRenderingKHR"));
    
    table.CmdPipelineBarrier = reinterpret_cast<PFN_vkCmdPipelineBarrier>(getDeviceProcAddr(device, "vkCmdPipelineBarrier"));
    table.CmdPipelineBarrier2 = reinterpret_cast<PFN_vkCmdPipelineBarrier2>(getDeviceProcAddr(device, "vkCmdPipelineBarrier2"));
    if (!table.CmdPipelineBarrier2) table.CmdPipelineBarrier2 = reinterpret_cast<PFN_vkCmdPipelineBarrier2>(getDeviceProcAddr(device, "vkCmdPipelineBarrier2KHR"));
    
    table.CmdWaitEvents = reinterpret_cast<PFN_vkCmdWaitEvents>(getDeviceProcAddr(device, "vkCmdWaitEvents"));
    table.CmdNextSubpass = reinterpret_cast<PFN_vkCmdNextSubpass>(getDeviceProcAddr(device, "vkCmdNextSubpass"));
    table.CreateImageView = reinterpret_cast<PFN_vkCreateImageView>(getDeviceProcAddr(device, "vkCreateImageView"));
    table.DestroyImageView = reinterpret_cast<PFN_vkDestroyImageView>(getDeviceProcAddr(device, "vkDestroyImageView"));
    table.CreateFramebuffer = reinterpret_cast<PFN_vkCreateFramebuffer>(getDeviceProcAddr(device, "vkCreateFramebuffer"));
    table.DestroyFramebuffer = reinterpret_cast<PFN_vkDestroyFramebuffer>(getDeviceProcAddr(device, "vkDestroyFramebuffer"));
    table.CmdClearDepthStencilImage = reinterpret_cast<PFN_vkCmdClearDepthStencilImage>(getDeviceProcAddr(device, "vkCmdClearDepthStencilImage"));

    // Sprint 5.5 Pointers
    table.CreateFence = reinterpret_cast<PFN_vkCreateFence>(getDeviceProcAddr(device, "vkCreateFence"));
    table.DestroyFence = reinterpret_cast<PFN_vkDestroyFence>(getDeviceProcAddr(device, "vkDestroyFence"));
    table.WaitForFences = reinterpret_cast<PFN_vkWaitForFences>(getDeviceProcAddr(device, "vkWaitForFences"));
    table.ResetFences = reinterpret_cast<PFN_vkResetFences>(getDeviceProcAddr(device, "vkResetFences"));
    table.CreateSemaphore = reinterpret_cast<PFN_vkCreateSemaphore>(getDeviceProcAddr(device, "vkCreateSemaphore"));
    table.DestroySemaphore = reinterpret_cast<PFN_vkDestroySemaphore>(getDeviceProcAddr(device, "vkDestroySemaphore"));
    table.CreateCommandPool = reinterpret_cast<PFN_vkCreateCommandPool>(getDeviceProcAddr(device, "vkCreateCommandPool"));
    table.DestroyCommandPool = reinterpret_cast<PFN_vkDestroyCommandPool>(getDeviceProcAddr(device, "vkDestroyCommandPool"));
    table.ResetCommandPool = reinterpret_cast<PFN_vkResetCommandPool>(getDeviceProcAddr(device, "vkResetCommandPool"));
    table.AllocateCommandBuffers = reinterpret_cast<PFN_vkAllocateCommandBuffers>(getDeviceProcAddr(device, "vkAllocateCommandBuffers"));
    table.FreeCommandBuffers = reinterpret_cast<PFN_vkFreeCommandBuffers>(getDeviceProcAddr(device, "vkFreeCommandBuffers"));
    table.BeginCommandBuffer = reinterpret_cast<PFN_vkBeginCommandBuffer>(getDeviceProcAddr(device, "vkBeginCommandBuffer"));
    table.EndCommandBuffer = reinterpret_cast<PFN_vkEndCommandBuffer>(getDeviceProcAddr(device, "vkEndCommandBuffer"));
    table.CmdBindPipeline = reinterpret_cast<PFN_vkCmdBindPipeline>(getDeviceProcAddr(device, "vkCmdBindPipeline"));
    table.CmdDispatch = reinterpret_cast<PFN_vkCmdDispatch>(getDeviceProcAddr(device, "vkCmdDispatch"));
    table.CmdCopyImage = reinterpret_cast<PFN_vkCmdCopyImage>(getDeviceProcAddr(device, "vkCmdCopyImage"));

    table.CreatePipelineCache = reinterpret_cast<PFN_vkCreatePipelineCache>(getDeviceProcAddr(device, "vkCreatePipelineCache"));
    table.DestroyPipelineCache = reinterpret_cast<PFN_vkDestroyPipelineCache>(getDeviceProcAddr(device, "vkDestroyPipelineCache"));
    table.CreateDescriptorSetLayout = reinterpret_cast<PFN_vkCreateDescriptorSetLayout>(getDeviceProcAddr(device, "vkCreateDescriptorSetLayout"));
    table.DestroyDescriptorSetLayout = reinterpret_cast<PFN_vkDestroyDescriptorSetLayout>(getDeviceProcAddr(device, "vkDestroyDescriptorSetLayout"));
    table.CreatePipelineLayout = reinterpret_cast<PFN_vkCreatePipelineLayout>(getDeviceProcAddr(device, "vkCreatePipelineLayout"));
    table.DestroyPipelineLayout = reinterpret_cast<PFN_vkDestroyPipelineLayout>(getDeviceProcAddr(device, "vkDestroyPipelineLayout"));
    table.CreateShaderModule = reinterpret_cast<PFN_vkCreateShaderModule>(getDeviceProcAddr(device, "vkCreateShaderModule"));
    table.DestroyShaderModule = reinterpret_cast<PFN_vkDestroyShaderModule>(getDeviceProcAddr(device, "vkDestroyShaderModule"));
    table.CreateComputePipelines = reinterpret_cast<PFN_vkCreateComputePipelines>(getDeviceProcAddr(device, "vkCreateComputePipelines"));
    table.CreateDescriptorPool = reinterpret_cast<PFN_vkCreateDescriptorPool>(getDeviceProcAddr(device, "vkCreateDescriptorPool"));
    table.DestroyDescriptorPool = reinterpret_cast<PFN_vkDestroyDescriptorPool>(getDeviceProcAddr(device, "vkDestroyDescriptorPool"));

    table.GetImageMemoryRequirements = reinterpret_cast<PFN_vkGetImageMemoryRequirements>(getDeviceProcAddr(device, "vkGetImageMemoryRequirements"));
    table.AllocateMemory = reinterpret_cast<PFN_vkAllocateMemory>(getDeviceProcAddr(device, "vkAllocateMemory"));
    table.FreeMemory = reinterpret_cast<PFN_vkFreeMemory>(getDeviceProcAddr(device, "vkFreeMemory"));
    table.BindImageMemory = reinterpret_cast<PFN_vkBindImageMemory>(getDeviceProcAddr(device, "vkBindImageMemory"));
    table.CreateSampler = reinterpret_cast<PFN_vkCreateSampler>(getDeviceProcAddr(device, "vkCreateSampler"));
    table.DestroySampler = reinterpret_cast<PFN_vkDestroySampler>(getDeviceProcAddr(device, "vkDestroySampler"));
    table.DestroyPipeline = reinterpret_cast<PFN_vkDestroyPipeline>(getDeviceProcAddr(device, "vkDestroyPipeline"));

    std::unique_lock lock(m_mutex);
    m_deviceTables[device] = table;
    m_deviceToInstance[device] = instance;
}

void VulkanDispatchTable::UnregisterDevice(VkDevice device) {
    std::unique_lock lock(m_mutex);
    m_deviceTables.erase(device);
    m_deviceToInstance.erase(device);
}

const InstanceDispatchTable* VulkanDispatchTable::GetInstanceDispatch(VkInstance instance) {
    std::shared_lock lock(m_mutex);
    auto it = m_instanceTables.find(instance);
    return it != m_instanceTables.end() ? &it->second : nullptr;
}

const DeviceDispatchTable* VulkanDispatchTable::GetDeviceDispatch(VkDevice device) {
    std::shared_lock lock(m_mutex);
    auto it = m_deviceTables.find(device);
    return it != m_deviceTables.end() ? &it->second : nullptr;
}

VkInstance VulkanDispatchTable::GetInstanceForDevice(VkDevice device) {
    std::shared_lock lock(m_mutex);
    auto it = m_deviceToInstance.find(device);
    return it != m_deviceToInstance.end() ? it->second : VK_NULL_HANDLE;
}

PFN_vkVoidFunction VKAPI_CALL VulkanDispatchTable::Hooked_vkGetInstanceProcAddr(VkInstance instance, const char* pName) {
    if (!pName) return nullptr;

    // Return our hook overrides
    if (strcmp(pName, "vkGetInstanceProcAddr") == 0) return reinterpret_cast<PFN_vkVoidFunction>(Hooked_vkGetInstanceProcAddr);
    if (strcmp(pName, "vkGetDeviceProcAddr") == 0) return reinterpret_cast<PFN_vkVoidFunction>(Hooked_vkGetDeviceProcAddr);
    if (strcmp(pName, "vkCreateInstance") == 0) return reinterpret_cast<PFN_vkVoidFunction>(hooks::Hooked_vkCreateInstance);
    if (strcmp(pName, "vkDestroyInstance") == 0) return reinterpret_cast<PFN_vkVoidFunction>(hooks::Hooked_vkDestroyInstance);
    if (strcmp(pName, "vkCreateDevice") == 0) return reinterpret_cast<PFN_vkVoidFunction>(hooks::Hooked_vkCreateDevice);
    if (strcmp(pName, "vkDestroyDevice") == 0) return reinterpret_cast<PFN_vkVoidFunction>(hooks::Hooked_vkDestroyDevice);
    if (strcmp(pName, "vkGetDeviceQueue") == 0) return reinterpret_cast<PFN_vkVoidFunction>(hooks::Hooked_vkGetDeviceQueue);
    if (strcmp(pName, "vkCreateSwapchainKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(hooks::Hooked_vkCreateSwapchainKHR);
    if (strcmp(pName, "vkDestroySwapchainKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(hooks::Hooked_vkDestroySwapchainKHR);
    if (strcmp(pName, "vkAcquireNextImageKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(hooks::Hooked_vkAcquireNextImageKHR);
    if (strcmp(pName, "vkQueueSubmit") == 0) return reinterpret_cast<PFN_vkVoidFunction>(hooks::Hooked_vkQueueSubmit);
    if (strcmp(pName, "vkQueuePresentKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(hooks::Hooked_vkQueuePresentKHR);
    if (strcmp(pName, "vkDestroySurfaceKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(hooks::Hooked_vkDestroySurfaceKHR);

    auto& dt = Get();
    if (dt.m_originalGetInstanceProcAddr) {
        return dt.m_originalGetInstanceProcAddr(instance, pName);
    }
    return nullptr;
}

PFN_vkVoidFunction VKAPI_CALL VulkanDispatchTable::Hooked_vkGetDeviceProcAddr(VkDevice device, const char* pName) {
    if (!pName) return nullptr;

    // Device level overrides
    if (strcmp(pName, "vkGetDeviceProcAddr") == 0) return reinterpret_cast<PFN_vkVoidFunction>(Hooked_vkGetDeviceProcAddr);
    if (strcmp(pName, "vkDestroyDevice") == 0) return reinterpret_cast<PFN_vkVoidFunction>(hooks::Hooked_vkDestroyDevice);
    if (strcmp(pName, "vkGetDeviceQueue") == 0) return reinterpret_cast<PFN_vkVoidFunction>(hooks::Hooked_vkGetDeviceQueue);
    if (strcmp(pName, "vkCreateSwapchainKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(hooks::Hooked_vkCreateSwapchainKHR);
    if (strcmp(pName, "vkDestroySwapchainKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(hooks::Hooked_vkDestroySwapchainKHR);
    if (strcmp(pName, "vkAcquireNextImageKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(hooks::Hooked_vkAcquireNextImageKHR);
    if (strcmp(pName, "vkQueueSubmit") == 0) return reinterpret_cast<PFN_vkVoidFunction>(hooks::Hooked_vkQueueSubmit);
    if (strcmp(pName, "vkQueuePresentKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(hooks::Hooked_vkQueuePresentKHR);

    // Sprint 5.2 overrides
    if (strcmp(pName, "vkCreateImage") == 0) return reinterpret_cast<PFN_vkVoidFunction>(hooks::Hooked_vkCreateImage);
    if (strcmp(pName, "vkDestroyImage") == 0) return reinterpret_cast<PFN_vkVoidFunction>(hooks::Hooked_vkDestroyImage);
    if (strcmp(pName, "vkCreateBuffer") == 0) return reinterpret_cast<PFN_vkVoidFunction>(hooks::Hooked_vkCreateBuffer);
    if (strcmp(pName, "vkDestroyBuffer") == 0) return reinterpret_cast<PFN_vkVoidFunction>(hooks::Hooked_vkDestroyBuffer);
    if (strcmp(pName, "vkBindBufferMemory") == 0) return reinterpret_cast<PFN_vkVoidFunction>(hooks::Hooked_vkBindBufferMemory);
    if (strcmp(pName, "vkMapMemory") == 0) return reinterpret_cast<PFN_vkVoidFunction>(hooks::Hooked_vkMapMemory);
    if (strcmp(pName, "vkUnmapMemory") == 0) return reinterpret_cast<PFN_vkVoidFunction>(hooks::Hooked_vkUnmapMemory);
    if (strcmp(pName, "vkAllocateDescriptorSets") == 0) return reinterpret_cast<PFN_vkVoidFunction>(hooks::Hooked_vkAllocateDescriptorSets);
    if (strcmp(pName, "vkFreeDescriptorSets") == 0) return reinterpret_cast<PFN_vkVoidFunction>(hooks::Hooked_vkFreeDescriptorSets);
    if (strcmp(pName, "vkUpdateDescriptorSets") == 0) return reinterpret_cast<PFN_vkVoidFunction>(hooks::Hooked_vkUpdateDescriptorSets);
    if (strcmp(pName, "vkResetDescriptorPool") == 0) return reinterpret_cast<PFN_vkVoidFunction>(hooks::Hooked_vkResetDescriptorPool);
    if (strcmp(pName, "vkCmdBindDescriptorSets") == 0) return reinterpret_cast<PFN_vkVoidFunction>(hooks::Hooked_vkCmdBindDescriptorSets);
    if (strcmp(pName, "vkCmdExecuteCommands") == 0) return reinterpret_cast<PFN_vkVoidFunction>(hooks::Hooked_vkCmdExecuteCommands);

    if (strcmp(pName, "vkCmdBeginRenderPass") == 0) return reinterpret_cast<PFN_vkVoidFunction>(hooks::Hooked_vkCmdBeginRenderPass);
    if (strcmp(pName, "vkCmdEndRenderPass") == 0) return reinterpret_cast<PFN_vkVoidFunction>(hooks::Hooked_vkCmdEndRenderPass);
    if (strcmp(pName, "vkCmdBeginRendering") == 0 || strcmp(pName, "vkCmdBeginRenderingKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(hooks::Hooked_vkCmdBeginRendering);
    if (strcmp(pName, "vkCmdEndRendering") == 0 || strcmp(pName, "vkCmdEndRenderingKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(hooks::Hooked_vkCmdEndRendering);
    
    if (strcmp(pName, "vkCmdPipelineBarrier") == 0) return reinterpret_cast<PFN_vkVoidFunction>(hooks::Hooked_vkCmdPipelineBarrier);
    if (strcmp(pName, "vkCmdPipelineBarrier2") == 0 || strcmp(pName, "vkCmdPipelineBarrier2KHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(hooks::Hooked_vkCmdPipelineBarrier2);
    if (strcmp(pName, "vkCmdWaitEvents") == 0) return reinterpret_cast<PFN_vkVoidFunction>(hooks::Hooked_vkCmdWaitEvents);
    if (strcmp(pName, "vkCmdNextSubpass") == 0) return reinterpret_cast<PFN_vkVoidFunction>(hooks::Hooked_vkCmdNextSubpass);
    
    if (strcmp(pName, "vkCreateImageView") == 0) return reinterpret_cast<PFN_vkVoidFunction>(hooks::Hooked_vkCreateImageView);
    if (strcmp(pName, "vkDestroyImageView") == 0) return reinterpret_cast<PFN_vkVoidFunction>(hooks::Hooked_vkDestroyImageView);
    if (strcmp(pName, "vkCreateFramebuffer") == 0) return reinterpret_cast<PFN_vkVoidFunction>(hooks::Hooked_vkCreateFramebuffer);
    if (strcmp(pName, "vkDestroyFramebuffer") == 0) return reinterpret_cast<PFN_vkVoidFunction>(hooks::Hooked_vkDestroyFramebuffer);
    if (strcmp(pName, "vkCmdClearDepthStencilImage") == 0) return reinterpret_cast<PFN_vkVoidFunction>(hooks::Hooked_vkCmdClearDepthStencilImage);

    auto& dt = Get();
    std::shared_lock lock(dt.m_mutex);
    auto it = dt.m_deviceToInstance.find(device);
    if (it != dt.m_deviceToInstance.end()) {
        auto instIt = dt.m_instanceTables.find(it->second);
        if (instIt != dt.m_instanceTables.end() && instIt->second.GetDeviceProcAddr) {
            return instIt->second.GetDeviceProcAddr(device, pName);
        }
    }
    
    // Fallback if device isn't registered yet or something
    if (dt.m_originalGetInstanceProcAddr && it != dt.m_deviceToInstance.end()) {
        return dt.m_originalGetInstanceProcAddr(it->second, pName);
    }

    return nullptr;
}

} // namespace vulkan
} // namespace vrinject
