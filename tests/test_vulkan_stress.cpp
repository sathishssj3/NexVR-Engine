#include <gtest/gtest.h>
#include <vulkan/vulkan.h>
#include <iostream>
#include <vector>
#include <chrono>
#include <atomic>

#include "../src/rendering/vulkan_graphics_backend.h"
#include "../src/rendering/vulkan_resource_state_tracker.h"
#include "../src/core/vulkan_lifecycle_manager.h"
#include "../src/core/vulkan_dispatch_table.h"
#include "../src/core/fault_injector.h"
#include "../src/rendering/stereo_pipeline.h"

using namespace vrinject;
using namespace vrinject::vulkan;

namespace {
    std::atomic<uint32_t> g_validationErrorCount{0};
    std::atomic<uint32_t> g_validationWarningCount{0};

    VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) 
    {
        if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
            g_validationErrorCount++;
            std::cerr << "[Validation Error] " << pCallbackData->pMessage << std::endl;
        } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
            g_validationWarningCount++;
            std::cerr << "[Validation Warning] " << pCallbackData->pMessage << std::endl;
        }
        return VK_FALSE;
    }
}

class VulkanStressTest : public ::testing::Test {
protected:
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
    bool hasRealVulkanDevice = false;

    void SetUp() override {
        FaultInjector::Clear();
        g_validationErrorCount.store(0);
        g_validationWarningCount.store(0);

        // 1. Create Vulkan Instance with VK_LAYER_KHRONOS_validation
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "NexVR Vulkan Stress Suite";
        appInfo.apiVersion = VK_API_VERSION_1_2;

        const char* layers[] = { "VK_LAYER_KHRONOS_validation" };
        const char* extensions[] = { VK_EXT_DEBUG_UTILS_EXTENSION_NAME };

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledLayerCount = 1;
        createInfo.ppEnabledLayerNames = layers;
        createInfo.enabledExtensionCount = 1;
        createInfo.ppEnabledExtensionNames = extensions;

        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            createInfo.enabledLayerCount = 0;
            if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
                GTEST_SKIP() << "No Vulkan runtime available";
                return;
            }
        }

        // Setup Debug Messenger
        auto vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        if (vkCreateDebugUtilsMessengerEXT) {
            VkDebugUtilsMessengerCreateInfoEXT debugInfo{};
            debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            debugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            debugInfo.pfnUserCallback = VulkanDebugCallback;
            vkCreateDebugUtilsMessengerEXT(instance, &debugInfo, nullptr, &debugMessenger);
        }

        // Select physical device
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (deviceCount == 0) {
            GTEST_SKIP() << "No Vulkan physical devices";
            return;
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
        physicalDevice = devices[0];

        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(physicalDevice, &props);
        std::cout << "[VulkanStressTest] GPU: " << props.deviceName << std::endl;

        // Create Logical Device
        float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = 0;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        VkDeviceCreateInfo devCreateInfo{};
        devCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        devCreateInfo.queueCreateInfoCount = 1;
        devCreateInfo.pQueueCreateInfos = &queueCreateInfo;

        if (vkCreateDevice(physicalDevice, &devCreateInfo, nullptr, &device) == VK_SUCCESS) {
            vkGetDeviceQueue(device, 0, 0, &queue);
            hasRealVulkanDevice = true;
        } else {
            GTEST_SKIP() << "Failed to create VkDevice";
            return;
        }

        // CRITICAL: Register the device in the VulkanDispatchTable so all subsystems
        // can resolve their Vulkan function pointers. Without this, every subsystem
        // call to GetDeviceDispatch() returns nullptr and the entire render path is dead.
        VulkanDispatchTable::Get().InitOriginalGetInstanceProcAddr(vkGetInstanceProcAddr);
        VulkanDispatchTable::Get().RegisterInstance(instance);
        VulkanDispatchTable::Get().RegisterDevice(device, instance);
    }

    void TearDown() override {
        FaultInjector::Clear();

        // Unregister from dispatch table before destroying Vulkan objects
        if (device) {
            VulkanDispatchTable::Get().UnregisterDevice(device);
        }
        if (instance) {
            VulkanDispatchTable::Get().UnregisterInstance(instance);
        }

        if (device) {
            vkDeviceWaitIdle(device);
            vkDestroyDevice(device, nullptr);
            device = VK_NULL_HANDLE;
        }
        if (debugMessenger && instance) {
            auto vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
            if (vkDestroyDebugUtilsMessengerEXT) {
                vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
            }
        }
        if (instance) {
            vkDestroyInstance(instance, nullptr);
            instance = VK_NULL_HANDLE;
        }
    }
};

TEST_F(VulkanStressTest, Render10kFramesFullRenderCopyPath) {
    ASSERT_TRUE(hasRealVulkanDevice) << "No real Vulkan device — cannot run stress test";
    ASSERT_NE(queue, VK_NULL_HANDLE) << "No queue handle";

    // Register lifecycle state so the backend sees a valid device
    auto& lifecycle = VulkanLifecycleManager::Get();
    lifecycle.OnInstanceCreated(instance, VK_API_VERSION_1_2);
    lifecycle.OnDeviceCreated(physicalDevice, device, nullptr);
    VkSwapchainCreateInfoKHR scInfo{};
    scInfo.surface = reinterpret_cast<VkSurfaceKHR>(0x5000);
    lifecycle.OnSwapchainCreated(device, reinterpret_cast<VkSwapchainKHR>(0x4000), &scInfo);

    VulkanGraphicsBackend backend;

    // Initialize with real device, physical device, queue, and queue family index
    bool initOk = backend.InitializeVulkan(device, physicalDevice, queue, 0);
    ASSERT_TRUE(initOk) << "Backend initialization failed — dispatch table or subsystem error";
    EXPECT_EQ(backend.GetState(), StereoRendererState::READY);

    VulkanResourceStateTracker* stateTracker = backend.GetStateTracker();
    ASSERT_NE(stateTracker, nullptr) << "StateTracker is null — subsystem init failed";

    // Get dispatch table for memory management
    auto dt = VulkanDispatchTable::Get().GetDeviceDispatch(device);
    ASSERT_NE(dt, nullptr);

    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);

    auto findMemType = [&](uint32_t bits, VkMemoryPropertyFlags flags) -> uint32_t {
        for (uint32_t i = 0; i < memProps.memoryTypeCount; i++)
            if ((bits & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & flags) == flags)
                return i;
        return UINT32_MAX;
    };

    // Create two 1920x1080 synthetic swapchain images backed by real device memory.
    // These simulate the XrSwapchainImageVulkanKHR targets that the runtime would provide.
    VkImageCreateInfo imgCi{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imgCi.imageType   = VK_IMAGE_TYPE_2D;
    imgCi.format      = VK_FORMAT_R8G8B8A8_UNORM;
    imgCi.extent      = {1920, 1080, 1};
    imgCi.mipLevels   = 1;
    imgCi.arrayLayers = 1;
    imgCi.samples     = VK_SAMPLE_COUNT_1_BIT;
    imgCi.tiling      = VK_IMAGE_TILING_OPTIMAL;
    imgCi.usage       = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkImage leftImage = VK_NULL_HANDLE, rightImage = VK_NULL_HANDLE;
    ASSERT_EQ(dt->CreateImage(device, &imgCi, nullptr, &leftImage),  VK_SUCCESS);
    ASSERT_EQ(dt->CreateImage(device, &imgCi, nullptr, &rightImage), VK_SUCCESS);

    auto allocBind = [&](VkImage img) -> VkDeviceMemory {
        VkMemoryRequirements req{};
        dt->GetImageMemoryRequirements(device, img, &req);
        uint32_t idx = findMemType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (idx == UINT32_MAX) return VK_NULL_HANDLE;
        VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        ai.allocationSize  = req.size;
        ai.memoryTypeIndex = idx;
        VkDeviceMemory mem = VK_NULL_HANDLE;
        if (dt->AllocateMemory(device, &ai, nullptr, &mem) != VK_SUCCESS) return VK_NULL_HANDLE;
        dt->BindImageMemory(device, img, mem, 0);
        return mem;
    };

    VkDeviceMemory leftMem  = allocBind(leftImage);
    VkDeviceMemory rightMem = allocBind(rightImage);
    ASSERT_NE(leftMem,  VK_NULL_HANDLE) << "Failed to allocate left swapchain image memory";
    ASSERT_NE(rightMem, VK_NULL_HANDLE) << "Failed to allocate right swapchain image memory";

    std::cout << "[VulkanStressTest] Synthetic swapchain images: left=" << leftImage
              << " right=" << rightImage << std::endl;

    // Seed the state tracker with the starting layout (UNDEFINED, same as XR runtime acquire)
    stateTracker->ForceResourceState(leftImage,  VK_IMAGE_LAYOUT_UNDEFINED,
                                     VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0);
    stateTracker->ForceResourceState(rightImage, VK_IMAGE_LAYOUT_UNDEFINED,
                                     VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0);

    // Reset barrier counter AFTER seeding, so only the frame loop's barriers are counted
    stateTracker->ResetBarrierCount();

    CameraSnapshot camSnapshot{};
    camSnapshot.view = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    camSnapshot.projection = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    camSnapshot.valid = true;
    camSnapshot.confidence = 1.0f;
    camSnapshot.resourceIdentity.width = 1920;
    camSnapshot.resourceIdentity.height = 1080;

    DepthSnapshot depthSnapshot{};
    depthSnapshot.confidence = 100.0f;
    depthSnapshot.identity.width = 1920;
    depthSnapshot.identity.height = 1080;

    StereoParams params{};
    params.ipd = 0.064f;

    const int FRAME_COUNT = 10000;

    std::cout << "[VulkanStressTest] Dispatching " << FRAME_COUNT
              << " frames (full render+copy path) under VK_LAYER_KHRONOS_validation..." << std::endl;

    auto startTime = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < FRAME_COUNT; ++i) {
        camSnapshot.frame = i;
        // Simulate XR runtime re-acquiring each frame's image as UNDEFINED layout
        stateTracker->ForceResourceState(leftImage,  VK_IMAGE_LAYOUT_UNDEFINED,
                                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0);
        stateTracker->ForceResourceState(rightImage, VK_IMAGE_LAYOUT_UNDEFINED,
                                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0);
        backend.SetOpenXRSwapchainImages(leftImage, rightImage);
        backend.RenderStereo(camSnapshot, depthSnapshot, params);
    }

    auto endTime   = std::chrono::high_resolution_clock::now();
    double loopSec = std::chrono::duration<double>(endTime - startTime).count();
    double loopMs  = loopSec * 1000.0;
    double avgUs   = (loopSec * 1e6) / FRAME_COUNT;
    uint64_t barriers = stateTracker->GetBarrierCount();

    std::cout << "\n========================================================" << std::endl;
    std::cout << " [Vulkan Stress Test — Sprint 5.6 Authoritative Report]" << std::endl;
    std::cout << "========================================================" << std::endl;
    std::cout << "  Frames Dispatched         : " << FRAME_COUNT << std::endl;
    std::cout << "  Total Wall-Clock          : " << loopMs << " ms" << std::endl;
    std::cout << "  Avg Dispatch Latency      : " << avgUs << " us/frame" << std::endl;
    std::cout << "  Real Barrier Emissions    : " << barriers
              << "  (VulkanResourceStateTracker::m_barrierCount)" << std::endl;
    std::cout << "  Validation Layer Errors   : " << g_validationErrorCount.load() << std::endl;
    std::cout << "  Validation Layer Warnings : " << g_validationWarningCount.load() << std::endl;
    std::cout << "========================================================" << std::endl;

    if (avgUs < 0.1)
        std::cout << "  ** WARNING: latency < 0.1us/frame — render path may be short-circuiting" << std::endl;
    if (barriers == 0)
        std::cout << "  ** WARNING: 0 barriers — TransitionImage never invoked" << std::endl;
    std::cout << "========================================================\n" << std::endl;

    EXPECT_EQ(g_validationErrorCount.load(), 0u) << "Validation layer errors detected";
    EXPECT_EQ(g_validationWarningCount.load(), 0u) << "Validation layer warnings detected";

    // Drain the queue before freeing memory: the last QueueSubmit is in-flight
    // without a fence (QueueWaitIdle-based sync), so we must wait here before
    // destroying the images or the validation layer fires VUID-vkFreeMemory-memory-00677.
    dt->QueueWaitIdle(queue);

    // Cleanup
    dt->FreeMemory(device, leftMem,  nullptr);
    dt->FreeMemory(device, rightMem, nullptr);
    dt->DestroyImage(device, leftImage,  nullptr);
    dt->DestroyImage(device, rightImage, nullptr);

    backend.Shutdown();
    EXPECT_EQ(backend.GetState(), StereoRendererState::UNINITIALIZED);
}
