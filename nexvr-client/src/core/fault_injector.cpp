#include "core/fault_injector.h"
#include <atomic>

namespace vrinject {

namespace {
    std::atomic<Dx11FaultPoint> g_activeFault{Dx11FaultPoint::NONE};
    std::atomic<VulkanFaultPoint> g_activeVulkanFault{VulkanFaultPoint::NONE};
}

void FaultInjector::InjectFault(Dx11FaultPoint fault) {
    g_activeFault.store(fault, std::memory_order_release);
}

bool FaultInjector::ShouldFail(Dx11FaultPoint point) {
    Dx11FaultPoint active = g_activeFault.load(std::memory_order_acquire);
    if (active == point) {
        // One-shot failure by default
        g_activeFault.store(Dx11FaultPoint::NONE, std::memory_order_release);
        return true;
    }
    return false;
}

void FaultInjector::InjectVulkanFault(VulkanFaultPoint fault) {
    g_activeVulkanFault.store(fault, std::memory_order_release);
}

bool FaultInjector::ShouldVulkanFail(VulkanFaultPoint point) {
    VulkanFaultPoint active = g_activeVulkanFault.load(std::memory_order_acquire);
    if (active == point) {
        g_activeVulkanFault.store(VulkanFaultPoint::NONE, std::memory_order_release);
        return true;
    }
    return false;
}

void FaultInjector::Clear() {
    g_activeFault.store(Dx11FaultPoint::NONE, std::memory_order_release);
    g_activeVulkanFault.store(VulkanFaultPoint::NONE, std::memory_order_release);
}

} // namespace vrinject
