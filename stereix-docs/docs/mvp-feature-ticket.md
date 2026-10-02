# MVP-001: Core DXGI Swapchain Hooking & Capability Registry

## 1. Goal & Context
*   **Why:** We must intercept the graphics pipeline in order to capture the frame texture, apply AI-driven depth processing, and forward the result to the VR headset via OpenXR.
*   **Who:** Core graphics engineers and users of the NexVR Engine.
*   **What makes it valuable:** This is the foundational feature of the injector. Without capturing the swapchain, we cannot manipulate the game's visuals.

---

## 2. Technical Stories
*   **Story 1:** As the engine, I need to intercept the `Present` method of the DXGI SwapChain to capture the backbuffer just before it is displayed to the monitor.
*   **Story 2:** As the engine, I need to intercept `ResizeBuffers` to handle game resolution changes gracefully without crashing the pipeline.
*   **Story 3:** As the `RuntimeState`, I need a capability registry that confirms DXGI hooking is active before attempting to initialize OpenXR stereo rendering.

---

## 3. Tech-Stack & Infrastructure Alignment
*   **Engine:** C++, MSVC x64.
*   **Libraries:** MinHook for detouring function pointers.
*   **Graphics:** DirectX 11 / DXGI.

---

## 4. Architectural Conventions
*   **Hook Lifecycle:** The `HookManager` must completely own the lifecycle of the MinHook instances. Individual hooks (`dx11_hook.cpp`) should merely register themselves.
*   **Atomic States:** Any shared state between the hooked game thread and our `RuntimeState` must use `std::atomic` to prevent deadlocks (DEAD-02/04).

---

## 5. Definition of Done (DoD)
To successfully close this issue, the implementation must meet the following criteria:
- [ ] **Functional MVP:** The injector successfully hooks a target D3D11 application and logs a message every time `Present` is called.
- [ ] **Resize Handling:** The hook correctly handles `ResizeBuffers` without resource leaks.
- [ ] **Lifecycle Safety:** `HookManager` cleanly detaches the hooks upon injector shutdown, preventing game crashes (DEAD-05).
- [ ] **Testing:** Include a C++ gtest verifying the hook capability registration.
- [ ] **Documentation Sync:**
    - [ ] Update `project-status.md` to move "DXGI / Swapchain Hooking" to *Completed*.
    - [ ] Append a version update log to `changelog.md`.
