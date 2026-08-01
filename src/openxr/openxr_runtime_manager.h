#pragma once

#include "openxr_types.h"
#include "openxr_health_monitor.h"
#include <string>
#include <vector>
#include "../core/graphics_types.h" // For GraphicsBackend

namespace vrinject {
namespace openxr {

class OpenXRRuntimeManager {
public:
    OpenXRRuntimeManager(OpenXRHealthMonitor* healthMonitor);
    ~OpenXRRuntimeManager();

    bool Initialize(const std::string& applicationName, GraphicsBackend backendAPI);
    bool CreateSession(ID3D11Device* device);
    bool CreateSessionDX12(ID3D12Device* device, ID3D12CommandQueue* commandQueue);
    bool CreateSessionVulkan(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex);
    
    // Retrieves the required adapter LUID for DX12
    bool CheckDX12GraphicsRequirements(LUID* outAdapterLuid);

    // Retrieves the required physical device for Vulkan
    bool CheckVulkanGraphicsRequirements(VkInstance instance, VkPhysicalDevice* outPhysicalDevice);

    // Retrieves Vulkan extension requirements
    bool GetVulkanInstanceExtensions(std::string& outExtensions);
    bool GetVulkanDeviceExtensions(std::string& outExtensions);

    void Shutdown();
    void PollEvents();

    RuntimeState GetState() const { return state_; }
    XrInstance GetInstance() const { return instance_; }
    XrSession GetSession() const { return session_; }
    XrSystemId GetSystemId() const { return systemId_; }
    XrSpace GetReferenceSpace() const { return referenceSpace_; }

private:
    bool CreateInstance(const std::string& appName, GraphicsBackend backendAPI);
    bool GetSystem();
    bool CreateReferenceSpace();

    void HandleSessionStateChanged(const XrEventDataSessionStateChanged& stateChangedEvent);
    
    OpenXRHealthMonitor* healthMonitor_ = nullptr;
    
    RuntimeState state_ = RuntimeState::UNINITIALIZED;
    
    XrInstance instance_ = XR_NULL_HANDLE;
    XrSystemId systemId_ = XR_NULL_SYSTEM_ID;
    XrSession session_ = XR_NULL_HANDLE;
    XrSpace referenceSpace_ = XR_NULL_HANDLE;
};

} // namespace openxr
} // namespace vrinject
