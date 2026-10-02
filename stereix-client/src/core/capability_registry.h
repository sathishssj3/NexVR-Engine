#pragma once
#include <atomic>

namespace vrinject {

enum class CapabilityFlag : uint32_t {
    None = 0,
    DX11Hooked = 1 << 0,
    DX12Hooked = 1 << 1,
    VulkanHooked = 1 << 2,
    OpenXRAvailable = 1 << 3,
    NativeCameraFound = 1 << 4,
    InputHooked = 1 << 5,
    AudioHooked = 1 << 6
};

inline CapabilityFlag operator|(CapabilityFlag a, CapabilityFlag b) {
    return static_cast<CapabilityFlag>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline CapabilityFlag operator&(CapabilityFlag a, CapabilityFlag b) {
    return static_cast<CapabilityFlag>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

class CapabilityRegistry {
public:
    CapabilityRegistry() : m_capabilities(0) {}

    void RegisterCapability(CapabilityFlag flag) {
        m_capabilities.fetch_or(static_cast<uint32_t>(flag), std::memory_order_relaxed);
    }

    void UnregisterCapability(CapabilityFlag flag) {
        m_capabilities.fetch_and(~static_cast<uint32_t>(flag), std::memory_order_relaxed);
    }

    bool HasCapability(CapabilityFlag flag) const {
        return (m_capabilities.load(std::memory_order_relaxed) & static_cast<uint32_t>(flag)) != 0;
    }

    void Reset() {
        m_capabilities.store(0, std::memory_order_relaxed);
    }

private:
    std::atomic<uint32_t> m_capabilities;
};

} // namespace vrinject
