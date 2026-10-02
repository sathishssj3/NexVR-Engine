# Subsystem: Graphics & Input Hooks (`src/hooks/`)

The **Hooks** subsystem intercepts the game's native graphics and input APIs using MinHook and Windows API detours, routing render passes and user inputs through NexVR Engine.

---

## Key Components

| Hook | File | Intercepted Calls | Purpose |
| :--- | :--- | :--- | :--- |
| **`DX11Hook`** | `dx11_hook.cpp` | `IDXGISwapChain::Present`, `ResizeBuffers`, `ID3D11DeviceContext::Draw*` | Captures the DX11 backbuffer, discovers depth buffers, and renders stereo VR views. |
| **`DX12Hook`** | `dx12_hook.cpp` | `ExecuteCommandLists`, `Present`, `ResizeBuffers`, `ResourceBarrier` | Intercepts DX12 direct command queues and swapchains. Safely releases backbuffer ComPtrs prior to `ResizeBuffers`. |
| **`VulkanHook` & `Layer`** | `vulkan_hook.cpp`, `vulkan_layer.cpp` | `vkQueuePresentKHR`, `vkCreateSwapchainKHR`, `vkCmdDraw*` | Full Vulkan implicit layer implementation intercepting swapchain images and command buffers. |
| **`DXGIFactoryHook`** | `dxgi_factory_hook.cpp` | `CreateSwapChain`, `CreateSwapChainForHwnd` | Intercepts swapchain creation to capture the game's direct command queue and DXGI factory. |
| **`InputHook` & `Manager`** | `input_hook.cpp`, `input_manager.cpp` | `GetCursorPos`, `SetCursorPos`, `GetRawInputData`, `XInputGetState` | Maps VR motion controller joysticks and gestures into native game mouse and gamepad inputs. |

---

## Golden Hooking Rules (Do Not Regress)

1. **MinHook Lifecycle Ownership**: `HookManager` in `src/core/hook_manager.cpp` owns `MH_Initialize` and `MH_Uninitialize`. Individual hooks must **never** call MinHook initialization or shutdown directly.
2. **Buffer Release on Resize**: When `ResizeBuffers` is called (during window resizing or resolution changes), all external references and SRVs attached to swapchain backbuffers must be released immediately before forwarding to the driver (`DXGI_ERROR_INVALID_CALL` prevention).
3. **Probe Swapchain Buffer Types**: Never assume an API based solely on factory queues. Always verify the swapchain's actual buffer interface (`probe.Get()`) to prevent DX11/DX12 misidentification.
