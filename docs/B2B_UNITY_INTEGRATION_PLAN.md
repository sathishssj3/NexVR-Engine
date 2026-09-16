# B2B Phase 2.0 — Unity Integration Plan

**Status:** native bridge landed and format question answered. Camera script drafted, never run.
**Artifacts:** `nexvr_unity_bridge.cpp` → `nexvr_unity.dll`, `NexVR.cs`, `NexVRCamera.cs`

---

## 1. The problem in one paragraph

`NexVR_SubmitFrameDX11` must run on the thread that owns the D3D11 immediate context. In Unity that is the **Render Thread**, and C# script code never runs there — a `MonoBehaviour` runs on the **Game Thread**. Unity's only sanctioned crossing is `CommandBuffer.IssuePluginEvent`, which schedules a native callback onto the Render Thread in command order. So the integration is shaped entirely by that one constraint.

---

## 2. Thread mapping

| Unity thread | SDK call | Why it belongs there |
|---|---|---|
| **Game Thread** (`Update`/`LateUpdate`) | `NexVR_GetGraphicsRequirements` | Must precede device creation entirely |
| **Game Thread** | `NexVR_InitializeDX11` | Needs the device; runs once |
| **Game Thread** | `NexVR_WaitFrame` | Blocks for the frame interval; produces head poses the simulation needs |
| **Game Thread** | `NexVR_Unity_StageFrameWithDepth` + `IssuePluginEvent` | Hands the token, color target, and optional depth buffer across |
| **Render Thread** | `NexVR_SubmitFrameWithDepthDX11` | Copy must be ordered against Unity's own draws on the immediate context |
| **Game Thread** | `NexVR_Unity_Detach` → `NexVR_Shutdown` | Detach first, always — see §6 |

### Per-frame sequence

```
GAME THREAD                          RENDER THREAD
───────────                          ─────────────
NexVR_WaitFrame(&state)
  └─ blocks ~11.8 ms (the throttle)
  └─ returns token + head poses
apply poses to the VR camera
render the scene to the RenderTexture

slot = StageFrameWithDepth(token, texPtr, depthPtr, nearZ, farZ, range)
  └─ -1 means skip this frame,
     never spin waiting
cmd.IssuePluginEvent(fn, slot)  ────►  OnRenderEvent(slot)
                                         ├─ read token + texture + depth
                                         └─ NexVR_SubmitFrameWithDepthDX11
                                               ├─ acquire  (no D3D lock)
                                               ├─ wait     (no D3D lock, finite)
                                               ├─ LOCK → CopyResource (color + depth) → UNLOCK
                                               └─ release, xrEndFrame
```

The token is what makes the split checkable. OpenXR requires strict 1:1 pairing between its frame-wait and frame-begin, and with those calls on different threads nothing else would catch a violation before the runtime hit undefined behaviour.

---

## 3. Why the bridge vendors no Unity headers

The conventional approach copies `IUnityInterface.h` and `IUnityGraphicsD3D11.h` into the plugin to obtain the `ID3D11Device`. This bridge does not:

- those headers ship with a Unity installation and carry Unity's licence terms, which creates a redistribution question for a file we barely need;
- `IUnityGraphicsD3D11` is a COM-style interface whose vtable we would have to match exactly, across Unity versions, forever.

Neither is necessary. **The render target Unity hands us already knows which device it was created on** — `ID3D11Texture2D::GetDevice` answers it. `NexVR_Unity_GetDeviceFromTexture` wraps that. The only Unity ABI the bridge depends on is the render-event signature, a plain `void __stdcall (int)` that has been stable for a decade.

The cost is that the plugin receives no Unity device-lifecycle events. That is handled explicitly instead: C# detaches before shutdown, and the SDK holds a device reference so a device cannot vanish mid-frame.

---

## 4. The four Phase 0.8 constraints, mapped to Unity

| Constraint | Unity status | How it is satisfied |
|---|---|---|
| Exact render target size | ✅ solvable | Create the `RenderTexture` from `RequiredTextureWidth/Height`. Do **not** recompute from the per-eye fields. |
| Format family | ✅ **measured** | `GraphicsFormat.R8G8B8A8_UNorm` / `_SRGB`. Never BGRA. See §5 |
| Colour space declared | ⚠️ **not what it looks like** | `NEXVR_COLOR_SPACE_SRGB_ENCODED` for an 8-bit target in **both** Unity colour space modes. Do **not** map from `QualitySettings.activeColorSpace` — that names the shading space, not the storage encoding. See §5 |
| Adapter LUID | ❌ **not solvable from inside a plugin** | See §7 |

---

## 5. The format trap

`RenderTextureFormat.ARGB32` does **not** guarantee `DXGI_FORMAT_R8G8B8A8_UNORM`. Unity chooses the underlying DXGI format, and on some paths it selects a **BGRA** layout.

Phase 0.8 measured D3D11 rejecting `R8G8B8A8_UNORM → B8G8R8A8_UNORM` outright: they are different typeless families despite both being 32-bit RGBA. So a Unity project can produce a render target that looks correct in the inspector and cannot be copied at all.

**Mitigation.** The SDK already picks the swapchain format from the runtime's offered list *preferring one the game's target can be copied into*, and returns `NEXVR_ERROR_INVALID_FORMAT` only when none can. The Unity layer should additionally prefer `GraphicsFormat.R8G8B8A8_UNorm` / `R8G8B8A8_SRGB` explicitly via `RenderTextureDescriptor.graphicsFormat` rather than the legacy `RenderTextureFormat` enum, which leaves the choice to Unity.

**MEASURED** (Unity 6000.5.9f1, D3D11, Linear colour space):

| Unity format | DXGI | Verdict |
|---|---|---|
| `RenderTextureFormat.Default` | 27 `R8G8B8A8_TYPELESS` | ✅ |
| `RenderTextureFormat.ARGB32` | 27 `R8G8B8A8_TYPELESS` | ✅ |
| `GraphicsFormat.R8G8B8A8_UNorm` | 27 `R8G8B8A8_TYPELESS` | ✅ **required** |
| `GraphicsFormat.R8G8B8A8_SRGB` | 27 `R8G8B8A8_TYPELESS` | ✅ **required** |
| `RenderTextureFormat.BGRA32` | 90 `B8G8R8A8_TYPELESS` | ❌ wrong family |
| `GraphicsFormat.B8G8R8A8_UNorm` | 90 `B8G8R8A8_TYPELESS` | ❌ wrong family |

Unity returns a TYPELESS resource so it can attach an sRGB view for writing. That is what makes the copy legal: `R8G8B8A8_TYPELESS` is castable within the R8G8B8A8 family, so it copies into an `R8G8B8A8_UNORM_SRGB` swapchain cleanly. Typeless is NOT universally compatible — `B8G8R8A8_TYPELESS` is still refused.

**Hard requirement: studios must use `GraphicsFormat.R8G8B8A8_UNorm` or `R8G8B8A8_SRGB`.**

And because the target is typeless with an sRGB view, the stored bytes are gamma-encoded even in a Linear project — so `NEXVR_COLOR_SPACE_SRGB_ENCODED` is correct in both Unity colour space modes. Mapping straight from `activeColorSpace` is wrong.

---

## 5b. Depth buffer & Reverse-Z submission

OpenXR runtimes supporting `XR_KHR_composition_layer_depth` can consume the application's depth buffer to perform precise reprojection and depth-tested composition against runtime overlays.

1. **Extraction:** Unity provides `RenderTexture.GetNativeDepthBufferPtr()`, returning the underlying `ID3D11Texture2D*` (typeless depth-stencil resource).
2. **Reverse-Z:** On Direct3D, Unity uses reversed depth buffers (`SystemInfo.usesReversedZBuffer == true`), where near is 1.0 and far is 0.0. When true, `NexVRDepthRange.Reversed` (1) is passed to `NexVR_Unity_StageFrameWithDepth`.
3. **Additive ABI:** Per DEC-020, `NexVR_Unity_StageFrame(token, texture)` remains byte-identical and exported for backwards compatibility, delegating internally to `NexVR_Unity_StageFrameWithDepth(..., nullptr, 0.0f, 0.0f, 0)`.
4. **Fallback:** If depth submission is disabled or the OpenXR runtime does not implement `XR_KHR_composition_layer_depth`, the bridge cleanly falls back to `NexVR_SubmitFrameDX11` without stalling or erroring.

---

## 6. Shutdown ordering

`NexVR_Unity_Detach` **before** `NexVR_Shutdown`, always.

`IssuePluginEvent` is asynchronous: when C# returns from issuing it, the Render Thread has not necessarily run the callback yet. Destroying the session first leaves a queued event to submit against freed memory. Detach clears the bridge's session pointer under a release store, so any event that runs afterwards finds `nullptr` and returns without touching it.

---

## 7. The adapter problem — the one we cannot solve in code

`NexVR_GetGraphicsRequirements` exists because **the runtime dictates which GPU the D3D11 device must live on**, and only reveals it after `xrCreateInstance`. Phase 0.8 established that a game whose device already exists on another adapter cannot be bound: the texture is on the wrong GPU and no copy reaches it.

**Unity creates its graphics device before any managed plugin code runs.** Even `[RuntimeInitializeOnLoadMethod(BeforeSplashScreen)]` is too late. So a Unity integration cannot obey this constraint the way a hand-written engine can.

What this means in practice:

- **Single-GPU machines: no issue.** There is one adapter and Unity picked it.
- **Multi-GPU machines (laptops with integrated + discrete, workstations with two cards): a real risk.** Unity may have selected the integrated GPU while the headset is wired to the discrete one.

**Mitigations, in order of preference:**

1. **Detect and explain.** `NexVR_InitializeDX11` already returns `NEXVR_ERROR_WRONG_ADAPTER` naming both LUIDs. The Unity layer must surface that as a launch-configuration problem, not a code bug.
2. **Launch-time selection.** Unity's `-force-device-index N` command-line argument selects the adapter. This is the actual fix, and it belongs in the integration guide and in the studio's launcher.
3. **OS-level GPU preference.** Windows Graphics Settings can pin an executable to the high-performance GPU.

**This is a documented limitation of the Unity adapter, not a defect to engineer around.** Writing code that silently tolerates the wrong adapter would mean shipping a black headset with no explanation — which is precisely the class of failure the whole SDK is built to eliminate.

---

## 8. `ID3D11Multithread` — a behaviour change studios must consent to

`NexVR_InitializeDX11` calls `SetMultithreadProtected(TRUE)` on the device, and reports through `NexVR_GetLastErrorDetail` whether NexVR was the one enabling it.

For Unity this is *usually* already on, but it must not be assumed. If NexVR turns it on, a lock has been inserted into the engine's context hot path. The SDK Integration Guide will state this outright rather than leaving a studio to discover it in a profiler.

---

## 9. What is deliberately not built yet

- **URP and HRP.** `NexVRCamera.cs` calls `Camera.Render()` explicitly, which only the Built-in pipeline supports. URP and HRP own the render loop and need their own injection points — a different hook, not a tweak.
- Editor tooling, package layout, `.meta` files.
- Any measurement inside a real Unity project. **Nothing in this document has been run against Unity.** The bridge compiles and its threading contract matches what Phase 0.9 measured, but the Unity half is design until a project exercises it.

---

## 10. Open questions

1. ~~Which DXGI format each `RenderTextureFormat` resolves to~~ — **answered**, §5. Only Unity 6000.5.9f1 / D3D11 / Linear measured; other versions unconfirmed.
2. Whether `RenderTexture.GetNativeTexturePtr()` is stable across frames or must be re-fetched — it forces a GPU sync on first call, and caching it wrongly is a use-after-free.
3. Whether URP/HRP expose a hook late enough to guarantee the scene is fully rendered when the plugin event is issued.
4. Whether Unity's own XR plugin can be left installed alongside this, or whether the two fight over the OpenXR runtime. Likely mutually exclusive; unverified.
