#include "vulkan_renderer.h"
#include "../../core/logger.h"
#include <vector>
#include <string>
#include <windows.h>
#ifndef NTSTATUS
typedef LONG NTSTATUS;
#endif
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")

#ifndef EXPECTED_SHADER_HASH
#define EXPECTED_SHADER_HASH L""
#endif

namespace {
std::wstring ComputeShaderHashSHA256(const uint8_t* data, size_t size) {
    BCRYPT_ALG_HANDLE hAlg = NULL;
    BCRYPT_HASH_HANDLE hHash = NULL;
    std::wstring hashResult = L"";
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0) != 0) return L"";

    DWORD cbData = 0, cbHashObject = 0;
    if (BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PBYTE)&cbHashObject, sizeof(DWORD), &cbData, 0) == 0) {
        std::vector<BYTE> pbHashObject(cbHashObject);
        DWORD cbHash = 0;
        if (BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PBYTE)&cbHash, sizeof(DWORD), &cbData, 0) == 0) {
            std::vector<BYTE> pbHash(cbHash);
            if (BCryptCreateHash(hAlg, &hHash, pbHashObject.data(), cbHashObject, NULL, 0, 0) == 0) {
                BCryptHashData(hHash, (PUCHAR)data, (ULONG)size, 0);
                if (BCryptFinishHash(hHash, pbHash.data(), cbHash, 0) == 0) {
                    wchar_t hex[3];
                    for (DWORD i = 0; i < cbHash; i++) {
                        swprintf_s(hex, L"%02X", pbHash[i]);
                        hashResult += hex;
                    }
                }
                BCryptDestroyHash(hHash);
            }
        }
    }
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return hashResult;
}
}

namespace vrinject {

bool VulkanRenderer::Initialize(void* nativeDevice, void* nativeContext) {
    if (!nativeDevice || !nativeContext) return false;
    
    // For Vulkan, nativeDevice is VkDevice, nativeContext is VkQueue
    m_device = static_cast<VkDevice>(nativeDevice);
    m_queue = static_cast<VkQueue>(nativeContext);

    // Create a command pool for our rendering operations
    // Note: In a real implementation we need the queue family index
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = 0; // Stub: need actual queue family
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_cmdPool) != VK_SUCCESS) {
        return false;
    }

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_cmdPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(m_device, &allocInfo, &m_cmdBuffer) != VK_SUCCESS) {
        return false;
    }

    // Create descriptor pool
    VkDescriptorPoolSize poolSize = {};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSize.descriptorCount = 100;

    VkDescriptorPoolCreateInfo descPoolInfo = {};
    descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descPoolInfo.poolSizeCount = 1;
    descPoolInfo.pPoolSizes = &poolSize;
    descPoolInfo.maxSets = 100;

    if (vkCreateDescriptorPool(m_device, &descPoolInfo, nullptr, &m_descPool) != VK_SUCCESS) {
        return false;
    }

    return true;
}

void VulkanRenderer::Shutdown() {
    if (m_descPool) { vkDestroyDescriptorPool(m_device, m_descPool, nullptr); m_descPool = VK_NULL_HANDLE; }
    if (m_cmdPool) { vkDestroyCommandPool(m_device, m_cmdPool, nullptr); m_cmdPool = VK_NULL_HANDLE; }
    m_device = VK_NULL_HANDLE;
    m_queue = VK_NULL_HANDLE;
}

TextureHandle VulkanRenderer::CreateTexture(uint32_t width, uint32_t height, uint32_t format, bool uav) {
    TextureHandle handle = {};
    if (!m_device) return handle;

    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = static_cast<VkFormat>(format);
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    if (uav) {
        imageInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    }
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkImage image = VK_NULL_HANDLE;
    if (vkCreateImage(m_device, &imageInfo, nullptr, &image) == VK_SUCCESS) {
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(m_device, image, &memRequirements);
        
        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = 0; // Stub: Requires VkPhysicalDeviceMemoryProperties
        
        VkDeviceMemory memory;
        if (vkAllocateMemory(m_device, &allocInfo, nullptr, &memory) == VK_SUCCESS) {
            vkBindImageMemory(m_device, image, memory, 0);
            
            handle.nativePtr = image;
            handle.width = width;
            handle.height = height;

            VkImageViewCreateInfo viewInfo = {};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = image;
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = imageInfo.format;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;

            VkImageView imageView = VK_NULL_HANDLE;
            if (vkCreateImageView(m_device, &viewInfo, nullptr, &imageView) == VK_SUCCESS) {
                handle.nativeView = imageView;
            }
        } else {
            vkDestroyImage(m_device, image, nullptr);
        }
    }
    return handle;
}

void VulkanRenderer::DestroyTexture(TextureHandle& handle) {
    if (handle.nativeView) {
        vkDestroyImageView(m_device, static_cast<VkImageView>(handle.nativeView), nullptr);
        handle.nativeView = nullptr;
    }
    if (handle.nativePtr) {
        vkDestroyImage(m_device, static_cast<VkImage>(handle.nativePtr), nullptr);
        handle.nativePtr = nullptr;
    }
}

ShaderHandle VulkanRenderer::LoadComputeShader(const uint8_t* bytecode, size_t bytecodeSize) {
    ShaderHandle handle = {};
    if (!m_device || !bytecode || bytecodeSize == 0) return handle;

    // S5.2: Cryptographic Shader Verification
    std::wstring expectedHash = EXPECTED_SHADER_HASH;
    if (!expectedHash.empty()) {
        std::wstring actualHash = ComputeShaderHashSHA256(bytecode, bytecodeSize);
        if (actualHash.empty() || _wcsicmp(actualHash.c_str(), expectedHash.c_str()) != 0) {
            LOG_ERROR("Vulkan Shader bytecode integrity check failed (SHA-256 mismatch)");
            return handle;
        }
    }

    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = bytecodeSize;
    createInfo.pCode = reinterpret_cast<const uint32_t*>(bytecode);

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(m_device, &createInfo, nullptr, &shaderModule) == VK_SUCCESS) {
        handle.shaderBytecode = shaderModule;
        
        VkPipelineLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        
        VkPipelineLayout pipelineLayout;
        if (vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &pipelineLayout) == VK_SUCCESS) {
            handle.rootSignature = pipelineLayout;
            
            VkComputePipelineCreateInfo pipelineInfo = {};
            pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
            pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
            pipelineInfo.stage.module = shaderModule;
            pipelineInfo.stage.pName = "CSMain";
            pipelineInfo.layout = pipelineLayout;
            
            VkPipeline pipeline;
            if (vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) == VK_SUCCESS) {
                handle.pipelineState = pipeline;
            }
        }
    }

    return handle;
}

void VulkanRenderer::DestroyShader(ShaderHandle& handle) {
    if (handle.pipelineState) {
        vkDestroyPipeline(m_device, static_cast<VkPipeline>(handle.pipelineState), nullptr);
        handle.pipelineState = nullptr;
    }
    if (handle.rootSignature) {
        vkDestroyPipelineLayout(m_device, static_cast<VkPipelineLayout>(handle.rootSignature), nullptr);
        handle.rootSignature = nullptr;
    }
    if (handle.shaderBytecode) {
        vkDestroyShaderModule(m_device, static_cast<VkShaderModule>(handle.shaderBytecode), nullptr);
        handle.shaderBytecode = nullptr;
    }
}

void VulkanRenderer::DispatchCompute(ShaderHandle shader, TextureHandle input, TextureHandle output, uint32_t groupsX, uint32_t groupsY) {
    if (!m_cmdBuffer || !shader.pipelineState) return;
    
    vkCmdBindPipeline(m_cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, static_cast<VkPipeline>(shader.pipelineState));
    // Note: VkDescriptorSet binding omitted for prototype
    vkCmdDispatch(m_cmdBuffer, groupsX, groupsY, 1);
}

void VulkanRenderer::CopyToSwapchain(TextureHandle source, void* swapchainTexture) {
    if (!m_cmdBuffer || !source.nativePtr || !swapchainTexture) return;

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(m_cmdBuffer, &beginInfo);

    VkImage srcImage = static_cast<VkImage>(source.nativePtr);
    VkImage dstImage = static_cast<VkImage>(swapchainTexture);

    VkImageCopy copyRegion = {};
    copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.srcSubresource.layerCount = 1;
    copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.dstSubresource.layerCount = 1;
    copyRegion.extent.width = source.width;
    copyRegion.extent.height = source.height;
    copyRegion.extent.depth = 1;

    // Image layout transitions
    VkImageMemoryBarrier barriers[2] = {};
    barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].image = srcImage;
    barriers[0].subresourceRange.aspectMask = copyRegion.srcSubresource.aspectMask;
    barriers[0].subresourceRange.baseMipLevel = copyRegion.srcSubresource.mipLevel;
    barriers[0].subresourceRange.levelCount = 1;
    barriers[0].subresourceRange.baseArrayLayer = copyRegion.srcSubresource.baseArrayLayer;
    barriers[0].subresourceRange.layerCount = copyRegion.srcSubresource.layerCount;
    barriers[0].srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].image = dstImage;
    barriers[1].subresourceRange.aspectMask = copyRegion.dstSubresource.aspectMask;
    barriers[1].subresourceRange.baseMipLevel = copyRegion.dstSubresource.mipLevel;
    barriers[1].subresourceRange.levelCount = 1;
    barriers[1].subresourceRange.baseArrayLayer = copyRegion.dstSubresource.baseArrayLayer;
    barriers[1].subresourceRange.layerCount = copyRegion.dstSubresource.layerCount;
    barriers[1].srcAccessMask = 0;
    barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(m_cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 2, barriers);

    vkCmdCopyImage(m_cmdBuffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 
                   dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    vkEndCommandBuffer(m_cmdBuffer);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_cmdBuffer;

    vkQueueSubmit(m_queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_queue); // Naive sync for prototype
}

} // namespace vrinject
