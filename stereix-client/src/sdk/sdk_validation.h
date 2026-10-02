// The five checks that stand between a studio and a black headset.
//
// ===========================================================================
// WHY THIS IS A SEPARATE TRANSLATION UNIT
// ===========================================================================
//
// Every constraint enforced here was discovered in Phase 0.8 by watching it be
// violated silently. CopyResource returns void; a wrong size, a wrong format,
// or a wrong adapter produces no error, no HRESULT, and no visible difference
// from a working integration until somebody puts a headset on.
//
// Phase 0.8 measured the cost of that: 30 frames submitted, every OpenXR call
// XR_SUCCESS, the compositor accepting all of them, and not one pixel copied.
//
// So these run ONCE, at initialization, before any of it can happen — and they
// are pure functions over plain data so the suite can exercise every failure
// path without a GPU. A validation layer that needs a headset to test is a
// validation layer that gets tested in production.

#pragma once

#include <d3d11.h>

#include <string>

#include "nexvr_sdk.h"

namespace nexvr::sdk {

/** What the runtime demanded, captured before the game had a device. */
struct RuntimeConstraints {
    LUID adapterLuid{};
    uint32_t requiredWidth{};
    uint32_t requiredHeight{};
    DXGI_FORMAT swapchainFormat{DXGI_FORMAT_UNKNOWN};
    bool queried{false};
};

/** A texture reduced to the properties a copy actually cares about. */
struct TextureFacts {
    uint32_t width{};
    uint32_t height{};
    DXGI_FORMAT format{DXGI_FORMAT_UNKNOWN};
    bool valid{false};

    /**
     * Multisample count. 1 for an ordinary texture.
     *
     * Added for depth, and left unread by the colour checks so that adding it
     * changed no existing verdict. CopyResource refuses a multisampled source,
     * and for depth there is no way out: ResolveSubresource does not accept
     * depth formats, so a resolve would have to be a shader pass this SDK does
     * not perform.
     */
    uint32_t sampleCount{1};
};

/** Whether a format is a depth or depth-stencil format. */
[[nodiscard]] bool IsDepthFormat(DXGI_FORMAT format) noexcept;

/**
 * The DXGI typeless format every concrete format is a view of.
 *
 * CopyResource permits a copy when source and destination share this parent —
 * which is why R8G8B8A8_UNORM into R8G8B8A8_UNORM_SRGB is legal and
 * R8G8B8A8_UNORM into B8G8R8A8_UNORM is not, despite both being 32-bit RGBA.
 * That second case matters more than it looks: BGRA is what a great many DXGI
 * swapchains use, so it is a realistic thing for a studio to hand us.
 *
 * Returns DXGI_FORMAT_UNKNOWN for a format with no typeless parent, which is
 * treated as "compatible only with itself".
 */
[[nodiscard]] DXGI_FORMAT TypelessParent(DXGI_FORMAT format) noexcept;

/** Whether CopyResource will accept these two formats. */
[[nodiscard]] bool CopyCompatible(DXGI_FORMAT source, DXGI_FORMAT destination) noexcept;

/**
 * Every initialization precondition, in one pure function.
 *
 * @param detail  Receives a human-readable explanation naming the specific
 *                mismatch and both values. The error CODE tells a program what
 *                went wrong; this tells a person how to fix it, which is the
 *                difference between a support ticket and a resolved one.
 *
 * @return NEXVR_SUCCESS, or the one code naming the first violated constraint.
 */
[[nodiscard]] NexVR_Result ValidateInitialize(const RuntimeConstraints& constraints,
                                              const TextureFacts& target,
                                              NexVR_ColorSpace colorSpace,
                                              const LUID& deviceLuid,
                                              bool contextIsImmediate,
                                              bool multithreadAvailable,
                                              std::string& detail);

/**
 * Every D3D12 initialization precondition, in one pure function.
 */
[[nodiscard]] NexVR_Result ValidateInitializeDX12(const RuntimeConstraints& constraints,
                                                  const TextureFacts& target,
                                                  NexVR_ColorSpace colorSpace,
                                                  const LUID& deviceLuid,
                                                  bool queueIsDirect,
                                                  std::string& detail);

// ===========================================================================
// PER-FRAME DECISIONS
// ===========================================================================
//
// The two functions below decide what the caller is told each frame. They live
// here, as pure functions over plain data, for the reason the file header gives
// and for one more that was learned the expensive way.
//
// NexVR_FrameState::views was declared in the public ABI, published with a
// viewCount of 2, and written by nothing. Every OpenXR call returned success,
// every frame was accepted, and the game read uninitialised stack memory as a
// head pose. The reason nobody caught it is that the copy lived inline in the
// ABI wrapper, where the only way to exercise it is to stand up a real device
// on a real runtime and then have something that CARES about poses look at the
// result. Nothing did.
//
// So the decision is a function now, and the test seeds views[] with a poison
// pattern and demands it be overwritten. That test fails against the code that
// shipped, which is the only property that makes a regression test worth
// having.

/**
 * Project one located frame onto the public NexVR_FrameState.
 *
 * Enforces the invariant documented on NexVR_FrameState: NEXVR_SUCCESS,
 * shouldRender, a non-zero viewCount, and written poses all move together.
 * Never writes views[] on any path that does not return NEXVR_SUCCESS, so a
 * caller polling with a reused struct cannot mistake last frame's poses — or
 * its own uninitialised memory — for this frame's.
 *
 * @param shouldRender  What the runtime said about this frame.
 * @param posesLocated  Whether xrLocateViews returned usable positions. False
 *                      demotes the frame to skipped even when shouldRender is
 *                      true: a frame worth displaying, rendered from a pose we
 *                      could not locate, is the accepted-but-wrong outcome that
 *                      no downstream check can catch.
 * @param viewCount     Entries in `views`. More than 2 is clamped, because the
 *                      public struct is fixed at 2 and silently writing past it
 *                      would be a buffer overrun in the caller's frame.
 * @param views         Located views. May be null when viewCount is 0.
 * @param out           Written in place. `structSize` is never modified.
 *
 * @return NEXVR_SUCCESS with poses, or NEXVR_FRAME_SKIPPED with viewCount 0.
 */
[[nodiscard]] NexVR_Result PublishFrameState(bool shouldRender, bool posesLocated,
                                             uint32_t viewCount, const NexVR_View* views,
                                             NexVR_FrameState& out) noexcept;

/**
 * Whether this texture may be submitted against a frame in this state.
 *
 * The null case is the whole point. A frame the runtime declined to display
 * still has to be submitted so that OpenXR ends the frame it began — but the
 * game has no finished render target for a frame it was told not to render, so
 * demanding one would mean keeping a dummy texture alive to satisfy an argument
 * that is never read.
 *
 * @param shouldRender  Whether the token's frame is being rendered.
 * @param submitted     DescribeTexture() of the submitted texture. A null
 *                      texture arrives here as `valid == false`.
 *
 * @retval NEXVR_SUCCESS                    Submit it.
 * @retval NEXVR_ERROR_INVALID_ARGUMENT     No texture on a rendered frame.
 * @retval NEXVR_ERROR_DIMENSION_MISMATCH   Not the size validated at init.
 */
[[nodiscard]] NexVR_Result ValidateSubmit(bool shouldRender, const TextureFacts& submitted,
                                          const RuntimeConstraints& constraints,
                                          std::string& detail);

/**
 * Every precondition on a depth submission, in one pure function.
 *
 * Depth is where a wrong value is least visible. A colour buffer of the wrong
 * size fails a copy loudly; a depth buffer with the near and far planes
 * transposed copies perfectly, submits successfully, and produces reprojection
 * that is subtly wrong in a way only somebody wearing a headset can see. So
 * these are checked rather than trusted, and checked here where every branch is
 * reachable without a GPU.
 *
 * @param structSize  The caller's `sizeof(NexVR_DepthInfoDX11)`. A value
 *                    smaller than this build's means the game was compiled
 *                    against an older header and its struct does not contain
 *                    the fields being read — refused rather than read past.
 * @param color       Facts about the colour texture in the same submission.
 *                    Depth must match its dimensions and sample count.
 * @param depth       Facts about the depth texture.
 * @param nearZ       Metres to the near plane. Must be finite and positive.
 * @param farZ        Metres to the far plane. Positive; INFINITY is allowed and
 *                    is what an infinite projection reports.
 *
 * @retval NEXVR_SUCCESS                   The depth may be submitted.
 * @retval NEXVR_ERROR_INVALID_ARGUMENT    structSize, clip planes, or MSAA.
 * @retval NEXVR_ERROR_DIMENSION_MISMATCH  Depth is not the size of colour.
 * @retval NEXVR_ERROR_INVALID_FORMAT      Not a depth format at all.
 */
[[nodiscard]] NexVR_Result ValidateDepthSubmit(uint32_t structSize, const TextureFacts& color,
                                               const TextureFacts& depth, float nearZ, float farZ,
                                               std::string& detail);

/**
 * Every precondition on a D3D12 depth submission, in one pure function.
 */
[[nodiscard]] NexVR_Result ValidateDepthSubmitDX12(uint32_t structSize, const TextureFacts& color,
                                                   const TextureFacts& depth, float nearZ, float farZ,
                                                   std::string& detail);

/**
 * Validates inputs for NexVR_GetControllerState.
 */
[[nodiscard]] NexVR_Result ValidateControllerState(NexVR_Hand hand,
                                                   const NexVR_ControllerState* state,
                                                   std::string& detail);

/**
 * Validates inputs for NexVR_TriggerHaptic.
 */
[[nodiscard]] NexVR_Result ValidateHapticFeedback(NexVR_Hand hand,
                                                  const NexVR_HapticFeedback* haptic,
                                                  std::string& detail);

}  // namespace nexvr::sdk
