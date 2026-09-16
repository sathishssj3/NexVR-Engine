# NexVR B2B SDK — Integration Guide

**Version:** 0.2.1 · DirectX 11 · Windows x64
**Audience:** the engineer wiring NexVR into an engine

---

## Read this first

This guide leads with the four things that will cost you a day if you discover them yourself. Every one of them was measured, not predicted, and every one produces a **silent** failure — a black headset, or a picture that is subtly wrong, with no error anywhere in your logs.

| # | Constraint | What it looks like when you get it wrong |
|---|---|---|
| 1 | The runtime picks the GPU | Init refuses with `NEXVR_ERROR_WRONG_ADAPTER`, and no code change fixes it |
| 2 | We enable `ID3D11Multithread` on your device | A lock appears in your context hot path |
| 3 | You must declare your colour space | The image renders, and looks wrong |
| 4 | `ClearState()` before you release | A D3D11 leak report that blames us |

The SDK is built so that #1, #3 and #4 fail loudly at initialization rather than quietly at runtime. That is deliberate, and it is why initialization asks for more than you might expect.

---

## 1. The shape of an integration

```c
// ── Startup, BEFORE your renderer creates its D3D11 device ──────────────
NexVR_GraphicsRequirements requirements = { sizeof(requirements) };
NexVR_GetGraphicsRequirements(&requirements);

//   → create your device on requirements.adapterLuid
//   → create your VR render target at
//     requirements.requiredTextureWidth × requiredTextureHeight

// ── Once, after the device and target exist ─────────────────────────────
NexVR_InitializeDX11Info info = { sizeof(info) };
info.device               = yourDevice;
info.immediateContext     = yourImmediateContext;
info.representativeTarget = yourVrRenderTarget;
info.colorSpace           = NEXVR_COLOR_SPACE_SRGB_ENCODED;  // or _LINEAR
NexVR_InitializeDX11(&info, &session);

// ── Per frame ───────────────────────────────────────────────────────────
// GAME / SIMULATION THREAD
NexVR_FrameState frame = { sizeof(frame) };
NexVR_WaitFrame(session, &frame);      // blocks ~one display interval
//   → apply frame.views[] to your cameras
//   → render your scene into the VR render target

// RENDER THREAD (the thread that owns the immediate context)
// Color-only submission:
NexVR_SubmitFrameDX11(session, frame.token, yourVrRenderTarget);

// Or submit with depth (optional, for positional timewarp & occlusion):
// NexVR_DepthInfoDX11 depth = { sizeof(depth) };
// depth.depthTexture = yourDepthTexture;
// depth.nearZ = 0.05f; depth.farZ = 1000.0f; depth.range = NEXVR_DEPTH_RANGE_REVERSED;
// NexVR_SubmitFrameWithDepthDX11(session, frame.token, yourVrRenderTarget, &depth);

// ── Shutdown ────────────────────────────────────────────────────────────
NexVR_Shutdown(session);
```

Every call returns a `NexVR_Result`. Check with `NEXVR_SUCCEEDED` / `NEXVR_FAILED`, **not** `== NEXVR_SUCCESS` — a dropped frame is a positive value and treating it as fatal will tear down a working session.

`NexVR_GetLastErrorDetail()` returns a sentence explaining the last failure **on the calling thread**. Log it. It is written for the person reading your logs at 2am, and it names the specific values that disagreed.

---

## 2. The multi-GPU constraint

### Why `NexVR_GetGraphicsRequirements` exists at all

It looks like an odd API. Most SDKs are handed your device and get on with it. This one demands to be called *before* your device exists, which inverts normal renderer startup.

The reason is not ours. **The OpenXR runtime dictates which physical GPU the D3D11 device must live on**, and it will only tell you after its own instance is created. If your device is already on a different adapter when you call us, we cannot bind to it — and this is not something copying can fix. Your render target physically resides in the wrong GPU's memory. There is no operation that reaches it.

So the ordering is:

```
xrCreateInstance  →  runtime names an adapter  →  you create your device there
```

and any integration that creates its device first has already lost.

### On a desktop with one GPU

Nothing to do. There is one adapter, you created your device on it, and the LUIDs match.

### On a laptop, or a workstation with two cards

This is a real problem and you will hit it.

A laptop has an integrated GPU and a discrete GPU. The headset is wired to the discrete one. Your engine may well have created its device on the integrated one — that is often the default, and for a flat game it is merely a performance question. For VR it is fatal.

You will see:

```
NEXVR_ERROR_WRONG_ADAPTER
The D3D11 device is on adapter LUID 00000000:00011134 but the OpenXR runtime
requires 00000000:000110F7. This cannot be corrected by copying — the render
target lives on the wrong GPU. Create the device on the required adapter.
```

### Fixing it

**In your own engine**, use `requirements.adapterLuid` to select the adapter when you call `D3D11CreateDevice`. Walk `IDXGIFactory1::EnumAdapters` and match the LUID, or use `IDXGIFactory4::EnumAdapterByLuid`.

**In Unity, you cannot.** Unity creates its graphics device before any managed plugin code runs — earlier than `[RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSplashScreen)]`, which is the earliest hook that exists. There is no code you can write inside a Unity project that runs first.

The fix is at launch:

```
YourGame.exe -force-device-index 1
```

Unity's `-force-device-index` selects the adapter by index. You will need to determine the right index for the machine — the SDK's error message gives you the LUID to match against `dxdiag` or `IDXGIFactory` enumeration.

**Ship this in your launcher.** A studio shipping a VR title on laptops must select the adapter at process start, in the launcher or shortcut, not hope the default is right.

**Alternative:** Windows *Settings → System → Display → Graphics* can pin an executable to the high-performance GPU. This works, but it is a per-machine setting a player has to apply, so it is a support instruction rather than a fix.

> We are not going to work around this in code. An SDK that quietly tolerated the wrong adapter would ship you a black headset with no explanation, which is worse than an error at startup that tells you exactly what to change.

---

## 3. The threading contract

### Where each call must run

| Call | Thread |
|---|---|
| `NexVR_GetGraphicsRequirements` | Any, during startup |
| `NexVR_InitializeDX11` | The thread owning the immediate context |
| `NexVR_WaitFrame` | Game / simulation thread |
| `NexVR_SubmitFrameDX11` | **The thread owning the immediate context** |
| `NexVR_Shutdown` | Any, with the other two joined |

`NexVR_WaitFrame` and `NexVR_SubmitFrameDX11` are **designed** to be on different threads. That is the normal pipelined-engine shape and the OpenXR specification explicitly supports it. What you must guarantee:

- calls to `NexVR_WaitFrame` are serialised with each other;
- every `frame.token` reaches exactly one `NexVR_SubmitFrameDX11`, in issue order.

The token exists so a violated pairing becomes `NEXVR_ERROR_INVALID_FRAME_TOKEN` instead of undefined behaviour inside the runtime.

### Why `NexVR_SubmitFrameDX11` must be on the render thread

Two independent reasons, and both matter:

1. **Ordering.** The copy is issued on the immediate context so D3D11 places it after your draws for that frame. A deferred context records a separate command list with no such ordering — the copy could execute against a half-drawn or previous frame. The SDK rejects a deferred context outright (`NEXVR_ERROR_NOT_IMMEDIATE_CONTEXT`) rather than let that happen.
2. **Thread safety.** `ID3D11DeviceContext` is not thread-safe. See below.

### `ID3D11Multithread` — a change to YOUR device

**`NexVR_InitializeDX11` calls `SetMultithreadProtected(TRUE)` on your device.** We are telling you plainly because it is a change to your engine's behaviour, not ours.

Why: we issue a `CopyResource` on your immediate context from the render thread, potentially while your renderer is issuing its own commands. A private mutex inside our SDK would serialise NexVR against NexVR and leave the real hazard untouched. `ID3D11Multithread` is the only lock your renderer can also be holding, which is exactly why it is the right one.

**If your engine deliberately runs the immediate context unsynchronised for performance, this inserts a lock into your hot path.**

After a successful `NexVR_InitializeDX11`, call `NexVR_GetLastErrorDetail()`. If NexVR was the one enabling protection, it returns:

> `NexVR enabled ID3D11Multithread on this device. If the renderer relied on the immediate context being unsynchronised, this inserts a lock into its hot path.`

If it returns empty, protection was already on and nothing changed. Most engines — Unity and Unreal among them — already enable it.

**Measured cost, for scale:** worst observed lock wait was **0.0059 ms** across 90-frame runs with a competing command stream on the same thread. That is not a meaningful number for your engine — our adversarial load was one thread issuing clears, not a real render graph — but it does mean the lock itself is not inherently expensive. Profile your own case.

### What we never do while holding that lock

The SDK acquires and waits for a compositor swapchain image **before** taking the D3D11 lock, and releases the lock before returning the image:

```
xrAcquireSwapchainImage        no D3D lock held
xrWaitSwapchainImage           no D3D lock held, finite deadline
  ID3D11Multithread::Enter()
    CopyResource
  ID3D11Multithread::Leave()
xrReleaseSwapchainImage / xrEndFrame
```

Blocking on the compositor while holding your context lock would turn a VR hiccup into a whole-engine deadlock — every thread of yours that needs the context would block behind us, including whichever one would run your shutdown path.

If the compositor does not free an image within the deadline, `NexVR_SubmitFrameDX11` returns **`NEXVR_ERROR_SWAPCHAIN_TIMEOUT`** and drops that frame. **This is survivable. Do not tear down your session.** A dropped frame is a flicker; a held lock is a reboot.

---

## 4. The colour space trap

`info.colorSpace` has no default and initialization refuses without it (`NEXVR_ERROR_COLOR_SPACE_UNSPECIFIED`). This is the field people most want to skip, so here is exactly what it protects you from.

**The copy performs no conversion.** Measured: copying `R8G8B8A8_UNORM` into `R8G8B8A8_UNORM_SRGB` is legal — the two share a typeless parent — and the bytes arrive **completely unchanged**. Byte `(234,150,150)` in your target is byte `(234,150,150)` in the swapchain.

What changes is how the compositor *interprets* those bytes. It treats the swapchain as sRGB-encoded and linearises it for display.

So:

- If your target holds **gamma-encoded, display-ready** values (typical of a final post-process output) → `NEXVR_COLOR_SPACE_SRGB_ENCODED`. Correct.
- If your target holds **linear** values → `NEXVR_COLOR_SPACE_LINEAR`. NexVR must convert; a plain copy would be wrong.

Guess wrong and **every API call still succeeds**. Every frame is accepted. The headset shows a picture that is washed out or crushed, and there is nothing in any log to tell you why. That is the entire reason this field is mandatory.

### Unity: do NOT map this from `activeColorSpace`

An earlier draft of this guide said `ColorSpace.Linear` → `NEXVR_COLOR_SPACE_LINEAR`. **That is wrong, and it is wrong in exactly the way this section warns about.**

Unity's colour space setting names the *working space shaders compute in*, not the encoding stored in the render target. In Linear mode Unity attaches an **sRGB view** to an 8-bit colour target, and the GPU converts linear → sRGB on write. The bytes that land in memory are gamma-encoded either way.

Measured, Unity 6000.5.9f1, project colour space **Linear**:

| Unity format | DXGI | Notes |
|---|---|---|
| `RenderTextureFormat.Default` | 27 `R8G8B8A8_TYPELESS` | typeless *so Unity can attach an sRGB view* |
| `RenderTextureFormat.ARGB32` | 27 `R8G8B8A8_TYPELESS` | |
| `GraphicsFormat.R8G8B8A8_UNorm` | 27 `R8G8B8A8_TYPELESS` | |
| `GraphicsFormat.R8G8B8A8_SRGB` | 27 `R8G8B8A8_TYPELESS` | |
| `RenderTextureFormat.BGRA32` | 90 `B8G8R8A8_TYPELESS` | **wrong family — will be refused** |
| `GraphicsFormat.B8G8R8A8_UNorm` | 90 `B8G8R8A8_TYPELESS` | **wrong family — will be refused** |

The typeless resource is what makes it work: `R8G8B8A8_TYPELESS` is castable **within the R8G8B8A8 family**, so a copy into an `R8G8B8A8_UNORM_SRGB` swapchain is legal. Note the narrowness of that — typeless is not universally compatible. A `B8G8R8A8_TYPELESS` target is still refused against an RGBA swapchain.

**So for a standard 8-bit Unity render target, use `NEXVR_COLOR_SPACE_SRGB_ENCODED` in both Gamma and Linear projects.** Reach for `NEXVR_COLOR_SPACE_LINEAR` only if you are handing us a float or HDR target holding genuinely linear values.

> **Verify this visually on first bring-up.** The reasoning above is sound and the format data is measured, but whether *your* pipeline's final target holds sRGB-encoded bytes has not been confirmed against a headset. If the image looks washed out or crushed, try the other value — it is a one-line change and it is the first thing to suspect.

### The related format trap

Your render target's format must share a **typeless parent** with the swapchain format, or the copy is rejected.

`R8G8B8A8_UNORM` and `B8G8R8A8_UNORM` are **different families**, despite both being 32-bit RGBA. A BGRA target against an RGBA swapchain is refused outright — and BGRA is what many DXGI swapchains use, so this is a realistic thing to hand us by accident.

The SDK chooses the swapchain format from the runtime's offered list *preferring one your target can be copied into*, and only returns `NEXVR_ERROR_INVALID_FORMAT` when none of them can. So you have latitude — but if you are choosing, choose `R8G8B8A8_UNORM` or `R8G8B8A8_UNORM_SRGB`.

**Unity, measured:** use `RenderTextureDescriptor.graphicsFormat` with `GraphicsFormat.R8G8B8A8_UNorm` or `R8G8B8A8_SRGB`. Both resolve to `R8G8B8A8_TYPELESS`, which is in the right family. **Never `BGRA32` or `B8G8R8A8_*`** — those resolve to `B8G8R8A8_TYPELESS` and will be refused. See the table below.

---

## 5. Depth buffer submission & Reverse-Z (`XR_KHR_composition_layer_depth`)

Submitting a depth buffer allows the OpenXR compositor to perform accurate **positional time-warp** (reprojecting against actual geometry rather than a flat plane) and correctly occlude runtime-drawn overlays (such as the SteamVR dashboard or controller pointers).

Depth submission is **entirely optional**:
- If your OpenXR runtime does not support `XR_KHR_composition_layer_depth`, NexVR transparently ignores the depth buffer and submits a color-only projection layer with zero overhead.
- If you do not have a depth buffer for a particular frame (e.g., 2D UI overlay or cutscene), pass `depth = NULL` to `NexVR_SubmitFrameWithDepthDX11`, or call `NexVR_SubmitFrameDX11`.
- Call `NexVR_SupportsDepthSubmission(session)` if your engine wants to check beforehand whether depth buffers are actually being consumed by the active runtime.

```c
// Optional: check if the runtime actually enabled depth submission
if (NexVR_SupportsDepthSubmission(session)) {
    NexVR_DepthInfoDX11 depth = { sizeof(depth) };
    depth.depthTexture = yourDepthTexture;
    depth.nearZ        = 0.05f;   // Distance in meters (positive)
    depth.farZ         = 1000.0f; // Distance in meters (positive) or INFINITY
    depth.range        = NEXVR_DEPTH_RANGE_REVERSED; // or NEXVR_DEPTH_RANGE_ZERO_TO_ONE

    NexVR_SubmitFrameWithDepthDX11(session, frame.token, yourVrRenderTarget, &depth);
} else {
    NexVR_SubmitFrameDX11(session, frame.token, yourVrRenderTarget);
}
```

### Reverse-Z vs Standard Depth Conventions

Modern engines (Unreal Engine, Unity HDRP, custom D3D11/D3D12 renderers) standardly use **Reverse-Z** (floating-point depth where the near plane is mapped to 1.0 and the far plane to 0.0) to dramatically improve depth precision and eliminate Z-fighting over vast distances.

NexVR accepts reverse-Z directly without attempting to "correct" or invert your clip planes:

| Convention | `nearZ` / `farZ` | `range` | Typical Depth Buffer Values |
|---|---|---|---|
| **Standard (0 → 1)** | `nearZ = 0.05f`, `farZ = 1000.0f` | `NEXVR_DEPTH_RANGE_ZERO_TO_ONE` | Near plane is `0.0`, Far plane is `1.0` |
| **Reverse-Z (1 → 0)** | `nearZ = 0.05f`, `farZ = 1000.0f` (or `1000.0f` / `0.05f`) | `NEXVR_DEPTH_RANGE_REVERSED` | Near plane is `1.0`, Far plane is `0.0` |
| **Infinite Far Plane** | `nearZ = 0.05f`, `farZ = INFINITY` | Set according to near plane value | Far plane at infinity |

> **Critical Rule on Viewport Depth Range:**
> `minDepth` and `maxDepth` in OpenXR describe the **viewport's value range** (normally `0.0f` to `1.0f`), **not** the clip planes. Do NOT swap `minDepth`/`maxDepth` to express Reverse-Z. State your clip planes in meters (`nearZ`, `farZ`) and set `range = NEXVR_DEPTH_RANGE_REVERSED`.

### Depth Texture Constraints

1. **Dimensions**: Must match `requiredTextureWidth` × `requiredTextureHeight` identically. A mismatch returns `NEXVR_ERROR_DIMENSION_MISMATCH`.
2. **Formats**: Must be a depth format supported by the runtime (e.g. `DXGI_FORMAT_D32_FLOAT`, `DXGI_FORMAT_D24_UNORM_S8_UINT`, `DXGI_FORMAT_D16_UNORM`) or share a typeless parent (e.g. `DXGI_FORMAT_R32_TYPELESS`, `DXGI_FORMAT_R24G8_TYPELESS`).
3. **No MSAA**: `CopyResource` cannot copy multisampled depth surfaces, and D3D11's `ResolveSubresource` does not accept depth formats. Depth buffers passed to NexVR must have `SampleDesc.Count == 1`.
4. **Shared Lock Protection**: Color and depth textures are copied inside the **same** `ID3D11Multithread` lock acquisition on your immediate context. This guarantees that game draw calls cannot interleave between the color and depth copies.
5. **structSize Validation**: Always initialize `depth.structSize = sizeof(NexVR_DepthInfoDX11)`. The SDK rejects undersized structs with `NEXVR_ERROR_INVALID_ARGUMENT` before reading any plane distances, guaranteeing cross-version ABI safety.

---

## 6. Shutdown, and a leak that is not ours

### Order

```
NexVR_Shutdown(session);   // releases OpenXR objects, then our device reference
// then your own teardown
```

`NexVR_Shutdown` destroys the OpenXR swapchain, space, session and instance in that order, and drops our reference to your device **last**. Reversing that would ask the runtime to clean up a session whose graphics binding had already gone.

Join your render thread before calling it. A submit already in flight against a destroyed session is a use-after-free, and no ordering inside our SDK can prevent it.

### The `ClearState()` leak

**This will be reported as a NexVR bug, and it will not be one.**

An `ID3D11DeviceContext` holds references to everything currently bound to the pipeline. If you release your render target while it is still bound, the texture stays alive — referenced by the context — and D3D11's leak report at device destruction lists it as a live object.

Because NexVR was the last thing touching that texture, the leak gets attributed to us. It is not. It is the pipeline's own reference.

**The fix is one line in your teardown:**

```cpp
context->ClearState();
context->Flush();
// now release your views, then your textures, then the context, then the device
```

Release order matters too — outward-in, so each `Release()` is the last one:

```
render target views
    ↓
textures
    ↓
ClearState() + Flush()
    ↓
immediate context
    ↓
device
```

### One thing that is safe, and surprises people

**A texture outliving the device that created it is fine.** Measured directly: after a game releases its device while an outside reference to a texture is held, the texture is still fully valid, still describes itself correctly, and still reaches its device via `GetDevice()`. D3D11 child objects keep their device alive through an internal reference; the device is not destroyed until the last child goes.

That property is what makes it safe for NexVR to borrow your textures at all.

---

## 7. Error codes

| Code | Meaning | What to do |
|---|---|---|
| `NEXVR_SUCCESS` | — | — |
| `NEXVR_SESSION_EXITING` | Runtime asked you to stop | Tear down, return to flat rendering |
| `NEXVR_FRAME_SKIPPED` | Do not render this frame | Skip it. Still submit the token. |
| `NEXVR_ERROR_RUNTIME_UNAVAILABLE` | No OpenXR runtime installed | Tell the user to install one |
| `NEXVR_ERROR_NO_HEADSET` | Runtime present, no system | Not a code problem |
| `NEXVR_ERROR_REQUIREMENTS_NOT_QUERIED` | Init before requirements query | Fix your startup order (§2) |
| `NEXVR_ERROR_WRONG_ADAPTER` | Device on the wrong GPU | §2. Not fixable at runtime. |
| `NEXVR_ERROR_DIMENSION_MISMATCH` | Target/depth is not the required size | Use `requiredTextureWidth/Height` verbatim |
| `NEXVR_ERROR_INVALID_FORMAT` | No compatible swapchain/depth format | §4 & §5 |
| `NEXVR_ERROR_NOT_IMMEDIATE_CONTEXT` | Deferred context supplied | §3 |
| `NEXVR_ERROR_COLOR_SPACE_UNSPECIFIED` | Colour space not stated | §4 |
| `NEXVR_ERROR_MULTITHREAD_UNAVAILABLE` | Could not get `ID3D11Multithread` | Report to us |
| `NEXVR_ERROR_INVALID_FRAME_TOKEN` | Token reused, stale, or out of order | §3 pairing rules |
| `NEXVR_ERROR_SWAPCHAIN_TIMEOUT` | Frame dropped | **Survivable.** Continue. |
| `NEXVR_ERROR_INVALID_ARGUMENT` | Stale structSize, invalid near/farZ, or MSAA depth | Check `structSize = sizeof(...)`, clip planes, sample count == 1 |

---

## 8. Bring-up: turn on copy verification

Set `info.enableCopyVerification = 1` while integrating.

The SDK then reads a pixel back out of the swapchain after every copy and fails if the copy did not land, instead of assuming it did.

This exists because of a real failure during our own development: 30 frames submitted, every OpenXR call returning success, the compositor accepting all of them, and **not one pixel copied**. `CopyResource` returns `void` — there is no error to check — and a dimension mismatch is a silent no-op. Nothing in the stack reported a problem.

It costs a full CPU/GPU sync per frame. **Turn it off before you ship.**

---

## 9. Things we have not verified

Stated plainly, because you will find them otherwise:

- **Runtimes other than SteamVR.** All measurements here are SteamVR/OpenXR 2.16.7. Oculus and WMR behaviour is unverified.
- **Frame pacing numbers.** Our timing data comes from a headless runtime with no display to synchronise against. Correctness findings hold; pacing numbers do not transfer.
- **A real compositor stall.** The timeout path is exercised by fault injection, not by a compositor that actually stalled.
- **DX12 and Vulkan.** Not supported. D3D11 only.
- **Unity colour encoding.** The format mapping is now measured (§4). Whether a given pipeline's final target actually holds sRGB-encoded bytes has not been confirmed against a headset.
- **Photometric / optical depth reprojection.** Verified end-to-end against live SteamVR (swapchain creation, 90/90 frames submitted with reverse-Z depth, 0 D3D11 debug errors). Physical lens verification requires a physical headset.

---

## 10. Diagnostics

`NEXVR_SDK_FAULT_INJECT_SWAPCHAIN_TIMEOUT=N` in the environment makes every Nth swapchain wait behave as a timeout. Use it to prove your engine survives dropped frames without waiting for a real compositor stall.

`NEXVR_SDK_FAULT_INJECT_FRAME_SKIP=N` makes every Nth frame report `NEXVR_FRAME_SKIPPED` from `NexVR_WaitFrame`, exercising the null-texture submit contract without needing runtime-driven frame throttle events.

These are diagnostic hooks, not API. Do not set them in a shipping build.
