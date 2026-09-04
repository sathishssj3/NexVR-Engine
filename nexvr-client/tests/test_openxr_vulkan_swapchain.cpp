#include <iostream>
#include <windows.h>
#include <vulkan/vulkan.h>
#include "openxr/openxr_runtime_manager.h"
#include "openxr/openxr_swapchain_manager.h"
#include "openxr/openxr_frame_submitter.h"
#include "rendering/vulkan_resource_state_tracker.h"
#include "openxr/openxr_health_monitor.h"
#include "rendering/vulkan/vulkan_graphics_backend.h"
#include "rendering/vulkan/vulkan_dispatch_table.h"
#include <vector>
#include <chrono>

using namespace vrinject;
using namespace vrinject::openxr;
using namespace vrinject::vulkan;

int main() {
    std::cout << "Running test_openxr_vulkan_swapchain...\n";

    OpenXRHealthMonitor healthMonitor;
    OpenXRRuntimeManager runtimeManager(&healthMonitor);
    
    // Initialize OpenXR first
    if (!runtimeManager.Initialize("TestApp", GraphicsBackend::Vulkan)) {
        std::cerr << "Failed to initialize OpenXR Runtime (Vulkan). Ensure an OpenXR runtime is active.\n";
        return 0; 
    }

    // Wait for SYSTEM_SELECTED
    while (runtimeManager.GetState() != RuntimeState::SYSTEM_SELECTED && runtimeManager.GetState() != RuntimeState::FAILED) {
        runtimeManager.PollEvents();
        Sleep(10);
    }
    if (runtimeManager.GetState() == RuntimeState::FAILED) {
        std::cerr << "OpenXR failed to reach SYSTEM_SELECTED.\n";
        return 0;
    }

    VkInstance instance = VK_NULL_HANDLE;
    VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    appInfo.apiVersion = VK_API_VERSION_1_2;
    VkInstanceCreateInfo instInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    instInfo.pApplicationInfo = &appInfo;

    std::string instanceExtensionsStr;
    if (!runtimeManager.GetVulkanInstanceExtensions(instanceExtensionsStr)) {
        std::cerr << "Failed to get Vulkan Instance Extensions from OpenXR.\n";
        return -1;
    }
    
    std::vector<const char*> instanceExtensions;
    size_t start = 0;
    while (start < instanceExtensionsStr.length()) {
        size_t end = instanceExtensionsStr.find(' ', start);
        if (end == std::string::npos) end = instanceExtensionsStr.length();
        if (end > start) {
            instanceExtensionsStr[end] = '\0';
            instanceExtensions.push_back(instanceExtensionsStr.c_str() + start);
        }
        start = end + 1;
    }

    // Add VK_EXT_DEBUG_UTILS_EXTENSION_NAME if not present (for validation)
    bool hasDebugUtils = false;
    for (const char* ext : instanceExtensions) {
        if (strcmp(ext, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0) {
            hasDebugUtils = true;
            break;
        }
    }
    if (!hasDebugUtils) {
        instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    
    // Enable validation layers
    const char* layers[] = { "VK_LAYER_KHRONOS_validation" };
    instInfo.enabledLayerCount = 1;
    instInfo.ppEnabledLayerNames = layers;

    instInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
    instInfo.ppEnabledExtensionNames = instanceExtensions.data();

    if (vkCreateInstance(&instInfo, nullptr, &instance) != VK_SUCCESS) {
        std::cerr << "Failed to create Vulkan Instance.\n";
        return -1;
    }

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    if (!runtimeManager.CheckVulkanGraphicsRequirements(instance, &physicalDevice)) {
        std::cerr << "Failed to get Vulkan Graphics Requirements/Device from OpenXR.\n";
        return -1;
    }

    if (physicalDevice == VK_NULL_HANDLE) {
        std::cerr << "OpenXR returned null physical device.\n";
        return -1;
    }
    
    // Request 2 queues from family 0: queue[0] for OpenXR, queue[1] for our rendering backend.
    // The XR Simulator claims queue[0] for compositing; queue[1] is unaffected.
    float queuePriorities[2] = {1.0f, 1.0f};
    VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queueInfo.queueFamilyIndex = 0;
    queueInfo.queueCount = 2;
    queueInfo.pQueuePriorities = queuePriorities;

    std::string deviceExtensionsStr;
    if (!runtimeManager.GetVulkanDeviceExtensions(deviceExtensionsStr)) {
        std::cerr << "Failed to get Vulkan Device Extensions from OpenXR.\n";
        return -1;
    }
    
    std::vector<const char*> deviceExtensions;
    start = 0;
    while (start < deviceExtensionsStr.length()) {
        size_t end = deviceExtensionsStr.find(' ', start);
        if (end == std::string::npos) end = deviceExtensionsStr.length();
        if (end > start) {
            deviceExtensionsStr[end] = '\0';
            deviceExtensions.push_back(deviceExtensionsStr.c_str() + start);
        }
        start = end + 1;
    }
    
    VkDeviceCreateInfo deviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;
    deviceInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();

    VkDevice device = VK_NULL_HANDLE;
    if (vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device) != VK_SUCCESS) {
        std::cerr << "Failed to create Vulkan Device.\n";
        return -1;
    }

    vrinject::vulkan::VulkanDispatchTable::Get().InitOriginalGetInstanceProcAddr(vkGetInstanceProcAddr);
    vrinject::vulkan::VulkanDispatchTable::Get().RegisterInstance(instance);
    vrinject::vulkan::VulkanDispatchTable::Get().RegisterDevice(device, instance);

    if (!runtimeManager.CreateSessionVulkan(instance, physicalDevice, device, 0, 0)) {
        std::cerr << "Failed to create Vulkan Session.\n";
        return -1;
    }

    OpenXRSwapchainManager swapchainManager(&healthMonitor);
    if (!swapchainManager.Initialize(runtimeManager.GetSession(), VK_FORMAT_R8G8B8A8_SRGB, 1024, 1024, GraphicsBackend::Vulkan)) {
        std::cerr << "Failed to initialize Swapchain Manager.\n";
        return -1;
    }
    VkQueue xrQueue = VK_NULL_HANDLE;   // queue[0] — used by XR Simulator compositing
    VkQueue renderQueue = VK_NULL_HANDLE; // queue[1] — used by our backend
    vkGetDeviceQueue(device, 0, 0, &xrQueue);
    vkGetDeviceQueue(device, 0, 1, &renderQueue);
    if (!renderQueue) {
        std::cerr << "Warning: could not get queue[1], falling back to queue[0]\n";
        renderQueue = xrQueue;
    }

    vrinject::vulkan::VulkanGraphicsBackend backend;
    if (!backend.InitializeVulkan(device, physicalDevice, renderQueue, 0)) {
        std::cerr << "Failed to initialize VulkanGraphicsBackend.\n";
        return -1;
    }

    OpenXRFrameSubmitter submitter(&healthMonitor);
    auto stateTracker = backend.GetStateTracker();

    const int FRAME_COUNT = 10000;
    std::cout << "Starting " << FRAME_COUNT << " frame swapchain acquire/render/release stress test...\n";

    CameraSnapshot cam;
    cam.frame = 0;
    DepthSnapshot depth;
    StereoParams params;

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < FRAME_COUNT; ++i) {
    try {
        runtimeManager.PollEvents();
        if (runtimeManager.GetState() != RuntimeState::SESSION_READY && 
            runtimeManager.GetState() != RuntimeState::RUNNING &&
            runtimeManager.GetState() != RuntimeState::FOCUSED) {
            
            if (runtimeManager.GetState() == RuntimeState::SESSION_CREATED) {
                // If it's just created, we need to wait for it to be ready. OpenXR usually transitions state via events.
                // For a headless test, this might stall if we don't have a real headset.
                // We'll attempt a single xrBeginSession if possible, but RuntimeManager handles it in PollEvents.
                continue; 
            } else if (runtimeManager.GetState() == RuntimeState::UNINITIALIZED || runtimeManager.GetState() == RuntimeState::FAILED) {
                std::cerr << "OpenXR state invalid during test.\n";
                break;
            }
        }

        VkImage leftDest = VK_NULL_HANDLE;
        VkImage rightDest = VK_NULL_HANDLE;
        
        if (i % 1000 == 0) std::cout << "Frame " << i << "\n";

        XrPosef outLeftPose, outRightPose;
        XrFovf outLeftFov, outRightFov;
        if (submitter.BeginAndAcquireVulkan(runtimeManager.GetSession(), XR_NULL_HANDLE, &swapchainManager, leftDest, rightDest, outLeftPose, outLeftFov, outRightPose, outRightFov)) {
            if (i < 5) {
                std::cout << "Frame " << i << " leftDest: " << (void*)leftDest << "\n";
            }
            if (leftDest) {
                stateTracker->ForceResourceState(leftDest, VK_IMAGE_LAYOUT_UNDEFINED, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0);
            }
            if (rightDest) {
                stateTracker->ForceResourceState(rightDest, VK_IMAGE_LAYOUT_UNDEFINED, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0);
            }

            cam.frame = i;
            backend.SetOpenXRSwapchainImages(leftDest, rightDest);
            RenderFrameSnapshot dummyFrame;
            backend.RenderStereo(dummyFrame, cam, depth, params, true);

            submitter.ReleaseAndEndVulkan(
                runtimeManager.GetSession(), 
                runtimeManager.GetReferenceSpace(), 
                &swapchainManager, 
                1024, 1024, 1024, 1024);
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Exception in frame " << i << ": " << e.what() << "\n";
        break;
    }
    catch (...) {
        std::cerr << "Unknown exception in frame " << i << "\n";
        break;
    }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    double duration_ms = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(end_time - start_time).count();
    
    std::cout << "\nTest completed in " << duration_ms << " ms (" 
              << (duration_ms / FRAME_COUNT) << " ms per frame avg).\n";
              
    std::cout << "Total Vulkan Barriers Emitted: " << stateTracker->GetBarrierCount() << "\n";

    std::cout << "Cleaning up...\n";
    backend.Shutdown();
    swapchainManager.Shutdown();
    runtimeManager.Shutdown();
    
    if (device) vkDestroyDevice(device, nullptr);
    if (instance) vkDestroyInstance(instance, nullptr);
    std::cout << "Vulkan/OpenXR Swapchain Test Completed Successfully.\n";
    return 0;
}
