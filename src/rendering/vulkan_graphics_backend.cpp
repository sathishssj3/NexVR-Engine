#include "vulkan_graphics_backend.h"
#include "stereo_pipeline.h"
#include "vulkan_stereo_resource_manager.h"
#include "vulkan_pipeline_cache.h"
#include "vulkan_descriptor_manager.h"
#include "vulkan_command_manager.h"
#include "vulkan_sync_manager.h"
#include "vulkan_resource_state_tracker.h"
#include "vulkan_stereo_renderer.h"
#include "../core/vulkan_dispatch_table.h"
#include "../core/stereo_camera_generator.h"
#include "../core/performance_profiler.h"
#include "../core/gpu_profiler.h"
#include "../core/runtime_state_monitor.h"
#include "stereo_pipeline.h"
#include "../openxr/openxr_runtime_manager.h"
#include "../openxr/openxr_swapchain_manager.h"
#include "../openxr/openxr_frame_submitter.h"
#include <iostream>

namespace vrinject {
namespace vulkan {

VulkanGraphicsBackend::VulkanGraphicsBackend() {
}

VulkanGraphicsBackend::~VulkanGraphicsBackend() {
    Shutdown();
}

bool VulkanGraphicsBackend::Initialize(void* nativeDevice, void* nativeContext) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_isInitialized) return true;

    m_device = static_cast<VkDevice>(nativeDevice);
    if (!m_device) return false;

    VkQueue queue = static_cast<VkQueue>(nativeContext);

    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(m_device);
    if (!dt) {
        std::cerr << "[VulkanGraphicsBackend] ERROR: Device not registered in VulkanDispatchTable." << std::endl;
        return false;
    }

    m_pipelineCache = std::make_unique<VulkanPipelineCache>(m_device);
    if (!m_pipelineCache->Initialize()) {
        std::cerr << "[VulkanGraphicsBackend] WARNING: PipelineCache init failed." << std::endl;
    }
    m_pipelineCache->EndInitializationPhase();

    m_resourceManager = std::make_unique<VulkanStereoResourceManager>(m_device, VK_NULL_HANDLE);

    VkDescriptorSetLayout dsLayout = m_pipelineCache->GetDescriptorSetLayout();
    m_descriptorManager = std::make_unique<VulkanDescriptorManager>(m_device, dsLayout);
    if (dsLayout) m_descriptorManager->AllocateSets(MAX_FRAMES_IN_FLIGHT);

    m_commandManager = std::make_unique<VulkanCommandManager>(m_device, 0);
    m_commandManager->Initialize();
    m_syncManager = std::make_unique<VulkanSyncManager>(m_device);
    m_syncManager->Initialize();
    m_stateTracker = std::make_unique<VulkanResourceStateTracker>(m_device);
    m_renderer = std::make_unique<VulkanStereoRenderer>(m_device, queue, 0);

    m_isInitialized = true;
    m_state = StereoRendererState::READY;
    return true;
}

bool VulkanGraphicsBackend::InitializeVulkan(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue queue, uint32_t queueFamilyIndex) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_isInitialized) return true;

    m_device = device;
    m_physicalDevice = physicalDevice;
    if (!m_device) return false;

    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(m_device);
    if (!dt) {
        std::cerr << "[VulkanGraphicsBackend] ERROR: Device not registered in VulkanDispatchTable." << std::endl;
        return false;
    }

    // 1. Pipeline Cache
    m_pipelineCache = std::make_unique<VulkanPipelineCache>(m_device);
    if (!m_pipelineCache->Initialize()) {
        std::cerr << "[VulkanGraphicsBackend] WARNING: PipelineCache init failed (shader missing?)." << std::endl;
    }
    m_pipelineCache->EndInitializationPhase();

    // 2. Resource Manager with real physical device
    m_resourceManager = std::make_unique<VulkanStereoResourceManager>(m_device, m_physicalDevice);
    if (m_physicalDevice) {
        if (!m_resourceManager->Resize(1920, 1080)) {
            std::cerr << "[VulkanGraphicsBackend] WARNING: ResourceManager Resize failed." << std::endl;
        }
    }

    // 3. Descriptor Manager
    VkDescriptorSetLayout dsLayout = m_pipelineCache->GetDescriptorSetLayout();
    m_descriptorManager = std::make_unique<VulkanDescriptorManager>(m_device, dsLayout);
    if (dsLayout) {
        if (!m_descriptorManager->AllocateSets(MAX_FRAMES_IN_FLIGHT)) {
            std::cerr << "[VulkanGraphicsBackend] WARNING: DescriptorManager AllocateSets failed." << std::endl;
        }
    }

    // 4. Command Manager
    m_commandManager = std::make_unique<VulkanCommandManager>(m_device, queueFamilyIndex);
    if (!m_commandManager->Initialize()) {
        std::cerr << "[VulkanGraphicsBackend] WARNING: CommandManager init failed." << std::endl;
    }

    // 5. Sync Manager & AI Queue
    m_syncManager = std::make_unique<VulkanSyncManager>(m_device);
    if (!m_syncManager->Initialize()) {
        std::cerr << "[VulkanGraphicsBackend] WARNING: SyncManager init failed." << std::endl;
    }
    // m_aiQueue = std::make_unique<AICommandQueue>();
    m_memoryBudget = std::make_unique<VulkanMemoryBudget>();

    // 6. Resource State Tracker
    m_stateTracker = std::make_unique<VulkanResourceStateTracker>(m_device);

    // 7. Stereo Renderer
    m_renderer = std::make_unique<VulkanStereoRenderer>(m_device, queue, queueFamilyIndex);

    // 8. Create GPU resources (camera buffer, depth image/view)
    if (m_physicalDevice) {
        if (!CreateGPUResources()) {
            std::cerr << "[VulkanGraphicsBackend] WARNING: CreateGPUResources failed." << std::endl;
        }
    }

    m_isInitialized = true;
    m_state = StereoRendererState::READY;
    return true;
}

bool VulkanGraphicsBackend::CreateGPUResources() {
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(m_device);
    if (!dt) return false;

    // --- Camera Uniform Buffer (128 bytes = 2x mat4) ---
    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = 128; // view + projection matrices
    bufInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (dt->CreateBuffer(m_device, &bufInfo, nullptr, &m_cameraBuffer) != VK_SUCCESS)
        return false;

    VkMemoryRequirements memReqs;
    // Use vkGetBufferMemoryRequirements via raw Vulkan (it's always available)
    vkGetBufferMemoryRequirements(m_device, m_cameraBuffer, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &m_cameraBufferMemory) != VK_SUCCESS)
        return false;
    vkBindBufferMemory(m_device, m_cameraBuffer, m_cameraBufferMemory, 0);

    // Write identity matrices into the buffer
    void* mapped = nullptr;
    vkMapMemory(m_device, m_cameraBufferMemory, 0, 128, 0, &mapped);
    float identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    memcpy(mapped, identity, 64);         // view matrix
    memcpy((char*)mapped + 64, identity, 64); // projection matrix
    vkUnmapMemory(m_device, m_cameraBufferMemory);

    // --- Depth Image (D32_SFLOAT, 1920x1080) ---
    VkImageCreateInfo imgInfo{};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.extent = {1920, 1080, 1};
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.format = VK_FORMAT_D32_SFLOAT;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imgInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (dt->CreateImage(m_device, &imgInfo, nullptr, &m_depthImage) != VK_SUCCESS)
        return false;

    VkMemoryRequirements imgMemReqs;
    dt->GetImageMemoryRequirements(m_device, m_depthImage, &imgMemReqs);

    VkMemoryAllocateInfo imgAlloc{};
    imgAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    imgAlloc.allocationSize = imgMemReqs.size;
    imgAlloc.memoryTypeIndex = FindMemoryType(imgMemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(m_device, &imgAlloc, nullptr, &m_depthImageMemory) != VK_SUCCESS)
        return false;
    vkBindImageMemory(m_device, m_depthImage, m_depthImageMemory, 0);

    // --- Depth Image View ---
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_depthImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_D32_SFLOAT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (dt->CreateImageView(m_device, &viewInfo, nullptr, &m_depthImageView) != VK_SUCCESS)
        return false;

    return true;
}

void VulkanGraphicsBackend::DestroyGPUResources() {
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(m_device);
    if (!dt) return;

    if (m_depthImageView) { dt->DestroyImageView(m_device, m_depthImageView, nullptr); m_depthImageView = VK_NULL_HANDLE; }
    if (m_depthImage) { dt->DestroyImage(m_device, m_depthImage, nullptr); m_depthImage = VK_NULL_HANDLE; }
    if (m_depthImageMemory) { vkFreeMemory(m_device, m_depthImageMemory, nullptr); m_depthImageMemory = VK_NULL_HANDLE; }
    if (m_cameraBuffer) { dt->DestroyBuffer(m_device, m_cameraBuffer, nullptr); m_cameraBuffer = VK_NULL_HANDLE; }
    if (m_cameraBufferMemory) { vkFreeMemory(m_device, m_cameraBufferMemory, nullptr); m_cameraBufferMemory = VK_NULL_HANDLE; }
}

uint32_t VulkanGraphicsBackend::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return 0;
}

void VulkanGraphicsBackend::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_isInitialized) return;

    WaitForGPU();

    m_renderer.reset();
    m_stateTracker.reset();
    m_syncManager.reset();
    m_commandManager.reset();
    m_descriptorManager.reset();
    m_pipelineCache.reset();
    m_resourceManager.reset();
    m_aiQueue.reset();
    m_memoryBudget.reset();
    DestroyGPUResources();

    m_isInitialized = false;
    m_state = StereoRendererState::UNINITIALIZED;
    m_device = VK_NULL_HANDLE;
}

void VulkanGraphicsBackend::Resize(uint32_t width, uint32_t height) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_isInitialized) return;
    WaitForGPU();
}

void VulkanGraphicsBackend::SetState(StereoRendererState state) { m_state = state; }
StereoRendererState VulkanGraphicsBackend::GetState() const { return m_state; }
bool VulkanGraphicsBackend::Healthy() const { return m_isInitialized && m_device != VK_NULL_HANDLE; }
CameraSnapshot VulkanGraphicsBackend::GetCamera() { return m_lastCamera; }
DepthSnapshot VulkanGraphicsBackend::GetDepth() { return m_lastDepth; }

void* VulkanGraphicsBackend::GetLeftEyeTexture() { return nullptr; }
void* VulkanGraphicsBackend::GetRightEyeTexture() { return nullptr; }

void VulkanGraphicsBackend::RenderStereo(const RenderFrameSnapshot& frameSnapshot, const CameraSnapshot& camSnapshot, const DepthSnapshot& depthSnapshot, const StereoParams& params) {

    StereoConstants constants;
    constants.ipd = params.ipd;
    constants.nearPlane = params.nearPlane;
    constants.farPlane = params.farPlane;
    constants.convergence = params.convergence;

    EyeView leftEye, rightEye;
    StereoCameraGenerator::Generate(frameSnapshot, camSnapshot, constants, leftEye, rightEye);

    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_isInitialized) return;

    m_lastCamera = camSnapshot;
    m_lastDepth = depthSnapshot;

    if (m_renderer && m_resourceManager && m_pipelineCache && m_descriptorManager && 
        m_commandManager && m_syncManager && m_stateTracker) {
        
        // Task 1: Async Fallback Contract
        uint64_t currentFrameId = camSnapshot.frame;
        m_state = StereoRendererState::RENDERING;

        // Use internally owned depth image if the snapshot doesn't provide one
        DepthSnapshot effectiveDepth = depthSnapshot;
        if (!effectiveDepth.identity.nativeHandle && m_depthImage) {
            effectiveDepth.identity.nativeHandle = m_depthImage;
        }

        bool ok = m_renderer->Render(camSnapshot, effectiveDepth,
                           m_cameraBuffer, 128, m_depthImageView,
                           *m_resourceManager, *m_pipelineCache, *m_descriptorManager,
                           *m_commandManager, *m_syncManager, *m_stateTracker,
                           m_oxrLeftDest, m_oxrRightDest);
        if (!ok && camSnapshot.frame < 10) {
            std::cerr << "[VulkanGraphicsBackend] RenderStereo: Render() returned false on frame " << camSnapshot.frame << std::endl;
        }
    }
}

void VulkanGraphicsBackend::SetOpenXRSwapchainImages(VkImage left, VkImage right) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_oxrLeftDest = left;
    m_oxrRightDest = right;
}

bool VulkanGraphicsBackend::CreateOpenXRSession(openxr::OpenXRRuntimeManager* xrRuntime, const RenderFrameSnapshot& snapshot) {
    if (!xrRuntime) return false;
    return xrRuntime->CreateSessionVulkan(
        static_cast<VkInstance>(snapshot.nativeInstance),
        static_cast<VkPhysicalDevice>(snapshot.nativePhysicalDevice),
        static_cast<VkDevice>(snapshot.nativeDevice),
        0, // queueFamilyIndex
        0  // queueIndex
    );
}

void VulkanGraphicsBackend::SubmitStereoFrame(
    openxr::OpenXRRuntimeManager* xrRuntime,
    openxr::OpenXRSwapchainManager* oxrSwapchain,
    openxr::OpenXRFrameSubmitter* oxrSubmitter,
    RenderFrameSnapshot& currentSnapshot,
    const CameraSnapshot& camSnapshot,
    const DepthSnapshot& depthSnapshot,
    const StereoParams& params,
    RuntimeStateMonitor& stateMonitor,
    PerformanceProfiler& cpuProfiler,
    GpuProfiler& gpuProfiler,
    bool shouldAttemptStereo
) {
    VkImage leftDest = VK_NULL_HANDLE;
    VkImage rightDest = VK_NULL_HANDLE;
    XrPosef leftPose, rightPose;
    XrFovf leftFov, rightFov;

    {
        ScopedCpuTimer oxrTimer(&cpuProfiler, CpuSegment::OpenXrSubmission);
        if (oxrSubmitter->BeginAndAcquireVulkan(xrRuntime->GetSession(),
                                                xrRuntime->GetReferenceSpace(),
                                                oxrSwapchain,
                                                leftDest, rightDest,
                                                leftPose, leftFov, rightPose, rightFov)) {
            currentSnapshot.leftPose = leftPose;
            currentSnapshot.leftFov = leftFov;
            currentSnapshot.rightPose = rightPose;
            currentSnapshot.rightFov = rightFov;

            // Ensure the tracker knows these external images are in UNDEFINED
            // layout before the first use
            if (leftDest) {
                m_stateTracker->ForceResourceState(
                    leftDest, VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0);
            }
            if (rightDest) {
                m_stateTracker->ForceResourceState(
                    rightDest, VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0);
            }

            SetOpenXRSwapchainImages(leftDest, rightDest);

            RenderStereo(currentSnapshot, camSnapshot, depthSnapshot, params);

            oxrSubmitter->ReleaseAndEndVulkan(
                xrRuntime->GetSession(), xrRuntime->GetReferenceSpace(),
                oxrSwapchain, currentSnapshot.width,
                currentSnapshot.height, currentSnapshot.width,
                currentSnapshot.height);

            stateMonitor.UpdateStereoHealth(true);
            stateMonitor.UpdateOpenXrHealth(true);
        } else {
            stateMonitor.UpdateOpenXrHealth(false);
        }
    }
}

VulkanResourceStateTracker* VulkanGraphicsBackend::GetStateTracker() const {
    return m_stateTracker.get();
}

void VulkanGraphicsBackend::WaitForGPU() {
    if (m_device) {
        auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(m_device);
        if (dt && dt->DeviceWaitIdle) {
            dt->DeviceWaitIdle(m_device);
        }
    }
}

} // namespace vulkan
} // namespace vrinject
