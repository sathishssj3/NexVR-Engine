#pragma once
#include <system_error>
// ============================================================================
// vrinject::BaseHook – Base class for DX hook implementations
//
/// @file base_hook.h
/// @brief Base class providing common functionality for DX11Hook and DX12Hook
///
/// This abstract base class extracts common functionality between DX11Hook and
/// DX12Hook to reduce code duplication, including:
// - Hook initialization and shutdown patterns
// - Deferred hook enabling mechanism
// - Hook integrity verification
// - Frame resource management infrastructure
// - Common Present hook processing framework
///
/// @thread_safety
/// Thread safety varies by implementation; see derived classes for specifics.
//
// ============================================================================

#ifndef VRINJECT_BASE_HOOK_H
#define VRINJECT_BASE_HOOK_H

#include <atomic>
#include <dxgi1_2.h>
#include <mutex>
#include <chrono>
#include <functional>
#include <MinHook.h>
#include "../rendering/irenderer.h"
#include "../openxr/openxr_runtime_manager.h"
#include "../core/config_manager.h"
#include "../core/logger.h"
#include "../core/seh_shield.h"

namespace vrinject {

// Forward declarations
struct FrameResources;

/**
 * @brief Base class for DirectX hook implementations
 *
 * Provides common functionality for DX11Hook and DX12Hook including:
 * - Hook lifecycle management (Initialize/Shutdown)
 * - Deferred hook enabling
 * - Hook integrity checking
 * - Basic frame resource management
 * - Present hook processing framework
 */
class BaseHook {
public:
    virtual ~BaseHook() = default;

    /**
     * @brief Initialize the hook system
     * @return true if initialization succeeded
     *
     * This template method implements the common initialization sequence:
     * 1. Initialize MinHook
     * 2. Create hook targets
     * 3. Enable primary hooks (Present)
     * 4. Leave deferred hooks disabled until device validation
     */
    virtual bool Initialize() = 0;

    /**
     * @brief Shutdown the hook system and cleanup resources
     */
    virtual void Shutdown() = 0;

    /**
     * @brief Get the renderer interface for this hook
     * @return Pointer to the renderer interface
     */
    virtual ::IRenderer* GetRenderer() = 0;

    /**
     * @brief Get the graphics API type this hook handles
     * @return GraphicsAPI enum value
     */
    virtual GraphicsAPI GetAPI() const = 0;

    /**
     * @brief Enable deferred hooks (called after device validation)
     *
     * This enables hooks that depend on having a valid device/context,
     * such as draw call hooks, after the initial Present call has validated
     * the device is ready.
     */
    virtual void EnableDeferredHooks() = 0;

    /**
     * @brief Check if deferred hooks are currently enabled
     * @return true if deferred hooks are enabled
     */
    virtual bool AreDeferredHooksEnabled() const = 0;

    /**
     * @brief Verify integrity of installed hooks
     *
     * Checks that hooks haven't been tampered with or removed by
     * anti-cheat systems or other overlays, and re-enables them if needed.
     */
    virtual void VerifyHookIntegrity() = 0;

protected:
    /**
     * @brief Constructor - protected to prevent direct instantiation
     */
    BaseHook()
        : m_deferredHooksEnabled{false}
    {}

    // Common helper functions for hook management

    /**
     * @brief Create and enable a MinHook
     * @param target Address of function to hook
     * @param detour Detour function to redirect to
     * @param original Pointer to store original function pointer
     * @param name Name of the function for logging
     * @return true if hook creation and enable succeeded
     */
    template<typename T>
    bool CreateAndEnableHook(void* target, T detour, void** original, const char* name) {
        if (!target) {
            LOG_ERROR("%s: Target address is null", name);
            return false;
        }

        if (MH_CreateHook(target, reinterpret_cast<void*>(detour), original) != MH_OK) {
            LOG_ERROR("MH_CreateHook failed for %s", name);
            return false;
        }

        if (MH_EnableHook(target) != MH_OK) {
            LOG_ERROR("MH_EnableHook failed for %s", name);
            return false;
        }

        LOG_INFO("%s hook created and enabled at %p", name, target);
        return true;
    }

    /**
     * @brief Create a MinHook without enabling it (for deferred hooks)
     * @param target Address of function to hook
     * @param detour Detour function to redirect to
     * @param original Pointer to store original function pointer
     * @param name Name of the function for logging
     * @return true if hook creation succeeded
     */
    template<typename T>
    bool CreateHook(void* target, T detour, void** original, const char* name) {
        if (!target) {
            LOG_ERROR("%s: Target address is null", name);
            return false;
        }

        if (MH_CreateHook(target, reinterpret_cast<void*>(detour), original) != MH_OK) {
            LOG_ERROR("MH_CreateHook failed for %s", name);
            return false;
        }

        LOG_INFO("%s hook created (disabled) at %p", name, target);
        return true;
    }

    /**
     * @brief Common Present hook processing template
     *
     * This template method handles the common aspects of Present hook processing:
     * - Device/swapchain change detection
     * - Backend initialization
     * - Resource management
     * - Frame callback execution
     *
     * Derived classes must implement the DX-specific parts.
     */
    template<typename SwapChainType, typename PresentFunc>
    HRESULT ProcessPresentImpl(
        SwapChainType* pSwapChain,
        PresentFunc originalFunc,
        UINT SyncInterval,
        UINT Flags,
        // DX11-specific params
        UINT pPresentParameters = 0, // Not used for DX12
        // Additional params can be added via template specialization
        bool isDX12 = false
    ) {
        // This is a simplified template - actual implementation would be in derived classes
        // The core idea is to extract common logic here

        // Validate parameters
        if (!pSwapChain || !originalFunc) {
            return originalFunc ? originalFunc(pSwapChain, SyncInterval, Flags,
                                          reinterpret_cast<const DXGI_PRESENT_PARAMETERS*>(pPresentParameters))
                               : DXGI_ERROR_INVALID_CALL;
        }

        try {
            // Common frame resource handling would go here
            // Device/swapchain change detection
            // Backend initialization
            // etc.

            // Call the original function (would be handled by derived class)
            return originalFunc(pSwapChain, SyncInterval, Flags,
                              reinterpret_cast<const DXGI_PRESENT_PARAMETERS*>(pPresentParameters));
        } catch (const std::system_error& e) {
            LOG_ERROR("Present system exception: %s (code: %d)", e.what(), e.code().value());
        } catch (const std::exception& e) {
            LOG_ERROR("Present exception caught: %s", e.what());
        } catch (...) {
            LOG_ERROR("Present unknown exception caught. Check VEH observer logs for SEH code.");
        }

        // Fallback to original function
        return originalFunc(pSwapChain, SyncInterval, Flags,
                          reinterpret_cast<const DXGI_PRESENT_PARAMETERS*>(pPresentParameters));
    }

    // Common state
    std::atomic<bool> m_deferredHooksEnabled;

    // Common helper for deferred hook enabling
    void SetDeferredHooksEnabled(bool enabled) {
        m_deferredHooksEnabled.store(enabled, std::memory_order_release);
    }
};

} // namespace vrinject

#endif // VRINJECT_BASE_HOOK_H