#include "rendering/vulkan/vulkan_graphics_backend.h"
#include "rendering/stereo/stereo_renderer.h"
#include <DirectXMath.h>
#include "rendering/stereo/stereo_pipeline.h"
#include "rendering/vulkan_stereo_resource_manager.h"
#include "rendering/vulkan_pipeline_cache.h"
#include "rendering/vulkan_descriptor_manager.h"
#include "rendering/vulkan_command_manager.h"
#include "rendering/vulkan_sync_manager.h"
#include "rendering/vulkan_resource_state_tracker.h"
#include "rendering/vulkan_stereo_renderer.h"
#include "rendering/vulkan/vulkan_dispatch_table.h"
#include "rendering/vulkan/vulkan_queue_manager.h"
#include "rendering/vulkan/vulkan_lifecycle_manager.h"
#include "rendering/stereo/stereo_camera_generator.h"
#include "core/performance_profiler.h"
#include "core/gpu_profiler.h"
#include "core/runtime_state_monitor.h"
#include "rendering/stereo/stereo_pipeline.h"
#include "openxr/openxr_runtime_manager.h"
#include "openxr/openxr_swapchain_manager.h"
#include "openxr/openxr_frame_submitter.h"
#include "rendering/vulkan/imgui_vulkan_integration.h"
#include "core/overlay_manager.h"
#include <iostream>

namespace vrinject {
namespace vulkan {

static Matrix4x4 InverseMatrix(const Matrix4x4& m) {
    DirectX::XMMATRIX dxm = DirectX::XMLoadFloat4x4(reinterpret_cast<const DirectX::XMFLOAT4X4*>(&m));
    DirectX::XMVECTOR det;
    DirectX::XMMATRIX invDxm = DirectX::XMMatrixInverse(&det, dxm);
    
    Matrix4x4 result;
    DirectX::XMStoreFloat4x4(reinterpret_cast<DirectX::XMFLOAT4X4*>(&result), invDxm);
    return result;
}

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

    // Retrieve the physical device from the lifecycle manager — the generic Initialize()
    // path (called from FrameCoordinator) didn't have it, which meant all GPU resource
    // allocation silently failed, leaving every VkImage/VkBuffer as VK_NULL_HANDLE and
    // producing a black screen in VR.
    m_physicalDevice = static_cast<VkPhysicalDevice>(
        VulkanLifecycleManager::Get().GetCurrentPhysicalDevice());
    if (!m_physicalDevice) {
        std::cerr << "[VulkanGraphicsBackend] WARNING: No physical device from lifecycle manager." << std::endl;
    }

    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(m_device);
    if (!dt) {
        std::cerr << "[VulkanGraphicsBackend] ERROR: Device not registered in VulkanDispatchTable." << std::endl;
        return false;
    }

    // Retrieve queue family index from QueueManager for correct command pool creation
    uint32_t queueFamilyIndex = 0;
    auto& qm = VulkanQueueManager::Get();
    if (auto record = qm.GetQueueRecord(queue)) {
        queueFamilyIndex = record->queueFamilyIndex;
    }

    m_pipelineCache = std::make_unique<VulkanPipelineCache>(m_device);
    if (!m_pipelineCache->Initialize()) {
        std::cerr << "[VulkanGraphicsBackend] WARNING: PipelineCache init failed." << std::endl;
    }
    m_pipelineCache->EndInitializationPhase();

    m_resourceManager = std::make_unique<VulkanStereoResourceManager>(m_device, m_physicalDevice);

    VkDescriptorSetLayout dsLayout = m_pipelineCache->GetDescriptorSetLayout();
    m_descriptorManager = std::make_unique<VulkanDescriptorManager>(m_device, dsLayout);
    if (dsLayout) m_descriptorManager->AllocateSets(MAX_FRAMES_IN_FLIGHT);

    m_commandManager = std::make_unique<VulkanCommandManager>(m_device, queueFamilyIndex);
    m_commandManager->Initialize();
    m_syncManager = std::make_unique<VulkanSyncManager>(m_device);
    m_syncManager->Initialize();
    m_stateTracker = std::make_unique<VulkanResourceStateTracker>(m_device);
    m_renderer = std::make_unique<VulkanStereoRenderer>(m_device, queue, queueFamilyIndex);
    m_memoryBudget = std::make_unique<VulkanMemoryBudget>();

    // Allocate GPU resources (camera uniform buffer, depth image) — without this the
    // stereo renderer has no buffers to write to and the VR output is black.
    if (m_physicalDevice) {
        if (!CreateGPUResources()) {
            std::cerr << "[VulkanGraphicsBackend] WARNING: CreateGPUResources failed." << std::endl;
        }
    }

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

    // --- Camera Uniform Buffer (256 bytes = StereoShaderConstants) ---
    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = 256; // StereoShaderConstants
    bufInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (dt->CreateBuffer(m_device, &bufInfo, nullptr, &m_cameraBuffer) != VK_SUCCESS)
        return false;

    VkMemoryRequirements memReqs;
    dt->GetBufferMemoryRequirements(m_device, m_cameraBuffer, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (dt->AllocateMemory(m_device, &allocInfo, nullptr, &m_cameraBufferMemory) != VK_SUCCESS)
        return false;
    dt->BindBufferMemory(m_device, m_cameraBuffer, m_cameraBufferMemory, 0);

    // Write zeros into the buffer initially
    void* mapped = nullptr;
    dt->MapMemory(m_device, m_cameraBufferMemory, 0, 256, 0, &mapped);
    memset(mapped, 0, 256);
    dt->UnmapMemory(m_device, m_cameraBufferMemory);

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

    if (dt->AllocateMemory(m_device, &imgAlloc, nullptr, &m_depthImageMemory) != VK_SUCCESS)
        return false;
    dt->BindImageMemory(m_device, m_depthImage, m_depthImageMemory, 0);

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
    if (m_depthImageMemory) { dt->FreeMemory(m_device, m_depthImageMemory, nullptr); m_depthImageMemory = VK_NULL_HANDLE; }
    if (m_gameColorView) { dt->DestroyImageView(m_device, m_gameColorView, nullptr); m_gameColorView = VK_NULL_HANDLE; m_gameColorImage = VK_NULL_HANDLE; }
    if (m_cameraBuffer) { dt->DestroyBuffer(m_device, m_cameraBuffer, nullptr); m_cameraBuffer = VK_NULL_HANDLE; }
    if (m_cameraBufferMemory) { dt->FreeMemory(m_device, m_cameraBufferMemory, nullptr); m_cameraBufferMemory = VK_NULL_HANDLE; }
}

uint32_t VulkanGraphicsBackend::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    auto instanceDt = VulkanDispatchTable::Get().GetInstanceDispatch(VulkanDispatchTable::Get().GetInstanceForDevice(m_device));
    if (!instanceDt) return 0;

    VkPhysicalDeviceMemoryProperties memProps;
    instanceDt->GetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProps);
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
    ImGuiVulkanIntegration::GetInstance().Shutdown();

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

void VulkanGraphicsBackend::RenderStereo(
    const RenderFrameSnapshot& frameSnapshot,
    const CameraSnapshot& camSnapshot, 
    const DepthSnapshot& depthSnapshot, 
    const StereoParams& params,
    bool shouldAttemptStereo,
    void* uiMaskHandle) 
{

    StereoConstants constants;
    constants.ipd = params.ipd;
    constants.nearPlane = params.nearPlane;
    constants.farPlane = params.farPlane;
    constants.convergence = params.convergence;
    constants.enableASW = params.enableASW;
    constants.aswTargetFps = params.aswTargetFps;

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

        // Ensure eye images are allocated to match the game's framebuffer size.
        // On the first frame (or after a resize), this allocates the left/right
        // eye VkImages that the compute shader writes into.
        if (frameSnapshot.width > 0 && frameSnapshot.height > 0) {
            m_resourceManager->Resize(frameSnapshot.width, frameSnapshot.height);
        }

        // Use internally owned depth image if the snapshot doesn't provide one
        DepthSnapshot effectiveDepth = depthSnapshot;
        if (!effectiveDepth.identity.nativeHandle && m_depthImage) {
            effectiveDepth.identity.nativeHandle = m_depthImage;
        }

        StereoShaderConstants shaderConsts{};
        shaderConsts.inverseViewProj = InverseMatrix(camSnapshot.vp);
        shaderConsts.leftViewProj = leftEye.viewProjection;
        shaderConsts.rightViewProj = rightEye.viewProjection;
        shaderConsts.originalEyePos = camSnapshot.position;
        shaderConsts.leftEyePos = leftEye.eyePosition;
        shaderConsts.rightEyePos = rightEye.eyePosition;
        shaderConsts.width = m_resourceManager->GetWidth();
        shaderConsts.height = m_resourceManager->GetHeight();
        shaderConsts.shouldAttemptStereo = shouldAttemptStereo ? 1 : 0;

        auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(m_device);
        if (!dt) return;

        void* mapped = nullptr;
        dt->MapMemory(m_device, m_cameraBufferMemory, 0, 256, 0, &mapped);
        if (mapped) {
            memcpy(mapped, &shaderConsts, sizeof(StereoShaderConstants));
            dt->UnmapMemory(m_device, m_cameraBufferMemory);
        }
        VkImage currentColorImage = static_cast<VkImage>(camSnapshot.resourceIdentity.nativeHandle);
        VkFormat viewFormat = static_cast<VkFormat>(camSnapshot.resourceIdentity.format);

        // Fallback: If camera heuristic failed, use the guaranteed backbuffer
        if (!currentColorImage && frameSnapshot.backBuffer) {
            currentColorImage = static_cast<VkImage>(frameSnapshot.backBuffer);
            viewFormat = static_cast<VkFormat>(44); // VK_FORMAT_B8G8R8A8_UNORM (standard swapchain)
        }

        if (currentColorImage && currentColorImage != m_gameColorImage) {
            if (m_gameColorView) {
                dt->DestroyImageView(m_device, m_gameColorView, nullptr);
                m_gameColorView = VK_NULL_HANDLE;
            }
            m_gameColorImage = currentColorImage;
            
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = m_gameColorImage;
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            
            if (viewFormat == 50) viewFormat = static_cast<VkFormat>(44); // B8G8R8A8_SRGB -> UNORM
            else if (viewFormat == 43) viewFormat = static_cast<VkFormat>(37); // R8G8B8A8_SRGB -> UNORM
            viewInfo.format = viewFormat;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;
            
            dt->CreateImageView(m_device, &viewInfo, nullptr, &m_gameColorView);
        }

        if (currentColorImage) {
            m_stateTracker->ForceResourceState(
                currentColorImage, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 
                VK_ACCESS_MEMORY_WRITE_BIT, 
                VK_QUEUE_FAMILY_IGNORED);
        }

        if (!m_gameColorView) return; // Cannot render without a color view

        bool ok = m_renderer->Render(camSnapshot, effectiveDepth,
                           m_cameraBuffer, 256, m_gameColorView, m_depthImageView ? m_depthImageView : m_gameColorView,
                           *m_resourceManager, *m_pipelineCache, *m_descriptorManager,
                           *m_commandManager, *m_syncManager, *m_stateTracker,
                           m_oxrLeftDest, m_oxrRightDest,
                           shouldAttemptStereo);
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

    uint32_t queueFamilyIndex = 0;
    uint32_t queueIndex = 0;
    
    auto& qm = vulkan::VulkanQueueManager::Get();
    VkQueue mainQueue = qm.GetMainGraphicsQueue();
    if (mainQueue) {
        if (auto record = qm.GetQueueRecord(mainQueue)) {
            queueFamilyIndex = record->queueFamilyIndex;
            queueIndex = record->queueIndex;
        }
    } else {
        std::cerr << "VulkanGraphicsBackend: GetMainGraphicsQueue returned NULL. Session creation may fail!" << std::endl;
    }

    return xrRuntime->CreateSessionVulkan(
        static_cast<VkInstance>(snapshot.nativeInstance),
        static_cast<VkPhysicalDevice>(snapshot.nativePhysicalDevice),
        static_cast<VkDevice>(snapshot.nativeDevice),
        queueFamilyIndex, // dynamically retrieved queueFamilyIndex
        queueIndex        // dynamically retrieved queueIndex
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
    bool shouldAttemptStereo,
    void* uiMaskHandle
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

            // Try stereo rendering first if the heuristic found a camera.
            // If stereo is not attempted or fails, fall back to the proven
            // direct copy path (monoscopic 2D in headset, zero format conversion).
            bool stereoOk = false;
            if (shouldAttemptStereo) {
                RenderStereo(currentSnapshot, camSnapshot, depthSnapshot, params, shouldAttemptStereo, uiMaskHandle);
                stereoOk = (GetState() == StereoRendererState::READY);
            }

            if (!stereoOk) {
            // Direct copy path: raw byte copy from game backbuffer to OpenXR swapchain.
            // Uses vkCmdCopyImage (not BlitImage) so there is zero format conversion
            // and zero gamma correction — the exact pixel data from the game is preserved.
            VkImage gameBackBuffer = static_cast<VkImage>(currentSnapshot.backBuffer);
            auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(m_device);

            if (gameBackBuffer && dt && (leftDest || rightDest)) {
                VkCommandBuffer cmd = VK_NULL_HANDLE;
                static uint32_t currentFrameIdx = 0;

                if (m_commandManager) {
                    if (m_syncManager) {
                        m_syncManager->WaitForFrame(currentFrameIdx);
                        m_syncManager->ResetFrame(currentFrameIdx);
                    }
                    cmd = m_commandManager->BeginFrame(currentFrameIdx);
                }

                if (cmd) {
                    // Transition game backbuffer: PRESENT_SRC -> TRANSFER_SRC
                    VkImageMemoryBarrier srcBarrier{};
                    srcBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                    srcBarrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                    srcBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                    srcBarrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
                    srcBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                    srcBarrier.image = gameBackBuffer;
                    srcBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    srcBarrier.subresourceRange.levelCount = 1;
                    srcBarrier.subresourceRange.layerCount = 1;
                    srcBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    srcBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

                    dt->CmdPipelineBarrier(cmd,
                        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        0, 0, nullptr, 0, nullptr, 1, &srcBarrier);

                    auto copyToEye = [&](VkImage dest) {
                        if (!dest) return;

                        // Transition dest: UNDEFINED -> TRANSFER_DST
                        VkImageMemoryBarrier dstBarrier{};
                        dstBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                        dstBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                        dstBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                        dstBarrier.srcAccessMask = 0;
                        dstBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                        dstBarrier.image = dest;
                        dstBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                        dstBarrier.subresourceRange.levelCount = 1;
                        dstBarrier.subresourceRange.layerCount = 1;
                        dstBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                        dstBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

                        dt->CmdPipelineBarrier(cmd,
                            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                            VK_PIPELINE_STAGE_TRANSFER_BIT,
                            0, 0, nullptr, 0, nullptr, 1, &dstBarrier);

                        // Raw byte copy — no format conversion, no gamma
                        // Game R8G8B8A8_UNORM → OpenXR R8G8B8A8_SRGB: same
                        // channel order, same texel size, preserves exact colors.
                        VkImageCopy copyRegion{};
                        copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                        copyRegion.srcSubresource.layerCount = 1;
                        copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                        copyRegion.dstSubresource.layerCount = 1;
                        copyRegion.extent.width = currentSnapshot.width;
                        copyRegion.extent.height = currentSnapshot.height;
                        copyRegion.extent.depth = 1;

                        dt->CmdCopyImage(cmd,
                            gameBackBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                            dest, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            1, &copyRegion);

                        // Transition dest: TRANSFER_DST -> COLOR_ATTACHMENT_OPTIMAL
                        VkImageMemoryBarrier finalBarrier{};
                        finalBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                        finalBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                        finalBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                        finalBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                        finalBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
                        finalBarrier.image = dest;
                        finalBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                        finalBarrier.subresourceRange.levelCount = 1;
                        finalBarrier.subresourceRange.layerCount = 1;
                        finalBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                        finalBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

                        dt->CmdPipelineBarrier(cmd,
                            VK_PIPELINE_STAGE_TRANSFER_BIT,
                            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                            0, 0, nullptr, 0, nullptr, 1, &finalBarrier);
                    };

                    copyToEye(leftDest);
                    copyToEye(rightDest);

                    // Transition game backbuffer back: TRANSFER_SRC -> PRESENT_SRC
                    VkImageMemoryBarrier restoreBarrier{};
                    restoreBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                    restoreBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                    restoreBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                    restoreBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                    restoreBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
                    restoreBarrier.image = gameBackBuffer;
                    restoreBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    restoreBarrier.subresourceRange.levelCount = 1;
                    restoreBarrier.subresourceRange.layerCount = 1;
                    restoreBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    restoreBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

                    dt->CmdPipelineBarrier(cmd,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                        0, 0, nullptr, 0, nullptr, 1, &restoreBarrier);

                    // End and submit
                    m_commandManager->EndFrame(currentFrameIdx);

                    VkFence fence = VK_NULL_HANDLE;
                    if (m_syncManager) {
                        fence = m_syncManager->GetFence(currentFrameIdx);
                    }

                    VkSubmitInfo submitInfo{};
                    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                    submitInfo.commandBufferCount = 1;
                    submitInfo.pCommandBuffers = &cmd;

                    VkQueue queue = static_cast<VkQueue>(currentSnapshot.nativeContext);
                    dt->QueueSubmit(queue, 1, &submitInfo, fence);

                    currentFrameIdx = (currentFrameIdx + 1) % 2;
                }
            }
            } // end if (!stereoOk)

            oxrSubmitter->ReleaseAndEndVulkan(
                xrRuntime->GetSession(), xrRuntime->GetReferenceSpace(),
                oxrSwapchain, currentSnapshot.width,
                currentSnapshot.height, currentSnapshot.width,
                currentSnapshot.height);

            stateMonitor.UpdateStereoHealth(stereoOk || (currentSnapshot.backBuffer != nullptr));
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
