#pragma once

namespace vrinject {

enum class Dx11FaultPoint {
    NONE,
    DISCOVER_DEVICE,
    DISCOVER_SWAPCHAIN,
    GET_BACKBUFFER,
    CREATE_RTV,
    RESIZE_BUFFERS,
    PRESENT_DEVICE_REMOVED,
    PRESENT_DEVICE_RESET,
    PRESENT_DEVICE_HUNG
};

enum class VulkanFaultPoint {
    NONE,
    CREATE_INSTANCE,
    CREATE_DEVICE,
    CREATE_SWAPCHAIN,
    QUEUE_SUBMIT_DEVICE_LOST,
    QUEUE_PRESENT_DEVICE_LOST,
    ALLOCATE_MEMORY_OOM,
    CREATE_COMPUTE_PIPELINE_FAILED
};

class FaultInjector {
public:
    static void InjectFault(Dx11FaultPoint fault);
    static bool ShouldFail(Dx11FaultPoint point);
    
    static void InjectVulkanFault(VulkanFaultPoint fault);
    static bool ShouldVulkanFail(VulkanFaultPoint point);

    static void Clear();
};

} // namespace vrinject
