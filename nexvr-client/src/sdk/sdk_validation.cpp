#include "sdk_validation.h"

#include <cmath>
#include <format>

namespace nexvr::sdk {

DXGI_FORMAT TypelessParent(DXGI_FORMAT format) noexcept {
    switch (format) {
        // --- 32-bit, four channels -----------------------------------------
        case DXGI_FORMAT_R8G8B8A8_TYPELESS:
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        case DXGI_FORMAT_R8G8B8A8_UINT:
        case DXGI_FORMAT_R8G8B8A8_SNORM:
        case DXGI_FORMAT_R8G8B8A8_SINT:
            return DXGI_FORMAT_R8G8B8A8_TYPELESS;

        // A SEPARATE family from R8G8B8A8, despite identical size and channel
        // count. This is the distinction that rejects a BGRA game target
        // against an RGBA swapchain.
        case DXGI_FORMAT_B8G8R8A8_TYPELESS:
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
            return DXGI_FORMAT_B8G8R8A8_TYPELESS;

        case DXGI_FORMAT_B8G8R8X8_TYPELESS:
        case DXGI_FORMAT_B8G8R8X8_UNORM:
        case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
            return DXGI_FORMAT_B8G8R8X8_TYPELESS;

        // --- 64-bit ---------------------------------------------------------
        case DXGI_FORMAT_R16G16B16A16_TYPELESS:
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
        case DXGI_FORMAT_R16G16B16A16_UNORM:
        case DXGI_FORMAT_R16G16B16A16_UINT:
        case DXGI_FORMAT_R16G16B16A16_SNORM:
        case DXGI_FORMAT_R16G16B16A16_SINT:
            return DXGI_FORMAT_R16G16B16A16_TYPELESS;

        // --- 128-bit --------------------------------------------------------
        case DXGI_FORMAT_R32G32B32A32_TYPELESS:
        case DXGI_FORMAT_R32G32B32A32_FLOAT:
        case DXGI_FORMAT_R32G32B32A32_UINT:
        case DXGI_FORMAT_R32G32B32A32_SINT:
            return DXGI_FORMAT_R32G32B32A32_TYPELESS;

        // --- 32-bit packed, HDR-ish ----------------------------------------
        case DXGI_FORMAT_R10G10B10A2_TYPELESS:
        case DXGI_FORMAT_R10G10B10A2_UNORM:
        case DXGI_FORMAT_R10G10B10A2_UINT:
            return DXGI_FORMAT_R10G10B10A2_TYPELESS;

        // R11G11B10_FLOAT has no typeless parent at all. Listed explicitly
        // rather than left to the default, so a reader can see it was
        // considered — it is a plausible engine HDR target, and it will only
        // ever copy to itself.
        case DXGI_FORMAT_R11G11B10_FLOAT:
            return DXGI_FORMAT_UNKNOWN;

        default:
            return DXGI_FORMAT_UNKNOWN;
    }
}

bool CopyCompatible(DXGI_FORMAT source, DXGI_FORMAT destination) noexcept {
    if (source == destination) {
        return true;
    }

    const DXGI_FORMAT sourceParent = TypelessParent(source);
    const DXGI_FORMAT destinationParent = TypelessParent(destination);

    // UNKNOWN on either side means "no family", and a format with no family is
    // compatible only with itself — already handled above. Returning false here
    // rather than treating UNKNOWN == UNKNOWN as a match is the difference
    // between rejecting two unrelated exotic formats and silently permitting
    // a copy between them.
    if (sourceParent == DXGI_FORMAT_UNKNOWN || destinationParent == DXGI_FORMAT_UNKNOWN) {
        return false;
    }

    return sourceParent == destinationParent;
}

NexVR_Result ValidateInitialize(const RuntimeConstraints& constraints, const TextureFacts& target,
                                NexVR_ColorSpace colorSpace, const LUID& deviceLuid,
                                bool contextIsImmediate, bool multithreadAvailable,
                                std::string& detail) {
    // Ordered by how early the mistake was made, so the first thing reported is
    // the first thing that went wrong. Telling somebody their format is wrong
    // when they are also on the wrong GPU sends them to fix the wrong thing.

    if (!constraints.queried) {
        detail =
            "NexVR_InitializeDX11 was called before NexVR_GetGraphicsRequirements. The adapter and "
            "render size are dictated by the runtime and are only knowable from that call, so it "
            "must run before the renderer creates its device.";
        return NEXVR_ERROR_REQUIREMENTS_NOT_QUERIED;
    }

    if (deviceLuid.LowPart != constraints.adapterLuid.LowPart ||
        deviceLuid.HighPart != constraints.adapterLuid.HighPart) {
        detail = std::format(
            "The D3D11 device is on adapter LUID {:08X}:{:08X} but the OpenXR runtime requires "
            "{:08X}:{:08X}. This cannot be corrected by copying — the render target lives on the "
            "wrong GPU. Create the device on the required adapter.",
            static_cast<uint32_t>(deviceLuid.HighPart), deviceLuid.LowPart,
            static_cast<uint32_t>(constraints.adapterLuid.HighPart), constraints.adapterLuid.LowPart);
        return NEXVR_ERROR_WRONG_ADAPTER;
    }

    if (!contextIsImmediate) {
        detail =
            "A deferred context was supplied. The copy must be ordered against the game's own draw "
            "calls, and a deferred context records an independent command list that carries no such "
            "ordering — the copy could execute against a previous or partially-drawn frame. Pass "
            "the immediate context.";
        return NEXVR_ERROR_NOT_IMMEDIATE_CONTEXT;
    }

    if (!target.valid) {
        detail = "The representative render target could not be inspected.";
        return NEXVR_ERROR_INVALID_ARGUMENT;
    }

    if (target.width != constraints.requiredWidth || target.height != constraints.requiredHeight) {
        detail = std::format(
            "The render target is {}x{} but the runtime requires exactly {}x{}. CopyResource "
            "cannot rescale; a mismatch here is silently ignored and produces a session that "
            "submits frames containing nothing. Size the target from "
            "NexVR_GraphicsRequirements::requiredTextureWidth/Height.",
            target.width, target.height, constraints.requiredWidth, constraints.requiredHeight);
        return NEXVR_ERROR_DIMENSION_MISMATCH;
    }

    // UNKNOWN is the normal case here, not a defect.
    //
    // The swapchain format cannot be enumerated without an XrSession, which
    // cannot exist without the device this validation precedes. So there is
    // genuinely nothing to compare against yet, and asserting one anyway is
    // what an earlier version did — it compared every target against
    // DXGI_FORMAT_UNKNOWN, failed every time, and masked the two checks below
    // it because they never ran.
    //
    // The constraint is still enforced, just later and better: Initialize picks
    // the swapchain format from the runtime's offered list PREFERRING one the
    // game's target can be copied into, and reports FailedOnFormat() only when
    // none of them can. Checking a known format here remains meaningful for a
    // caller that supplied one.
    if (constraints.swapchainFormat != DXGI_FORMAT_UNKNOWN &&
        !CopyCompatible(target.format, constraints.swapchainFormat)) {
        detail = std::format(
            "Render target format {} cannot be copied into swapchain format {}: they do not share a "
            "typeless parent ({} vs {}). Note that R8G8B8A8 and B8G8R8A8 are different families "
            "despite both being 32-bit RGBA.",
            static_cast<int>(target.format), static_cast<int>(constraints.swapchainFormat),
            static_cast<int>(TypelessParent(target.format)),
            static_cast<int>(TypelessParent(constraints.swapchainFormat)));
        return NEXVR_ERROR_INVALID_FORMAT;
    }

    if (colorSpace == NEXVR_COLOR_SPACE_UNSPECIFIED) {
        // Last, and never defaulted. Every check above rejects something that
        // would have failed loudly-ish somewhere; this one rejects something
        // that would have SUCCEEDED and looked wrong.
        detail =
            "colorSpace was not specified. There is no safe default: CopyResource performs no "
            "conversion, so bytes written as linear and interpreted as sRGB (or the reverse) "
            "produce a visibly wrong image while every call in the pipeline reports success. "
            "State NEXVR_COLOR_SPACE_SRGB_ENCODED or NEXVR_COLOR_SPACE_LINEAR.";
        return NEXVR_ERROR_COLOR_SPACE_UNSPECIFIED;
    }

    if (!multithreadAvailable) {
        detail =
            "ID3D11Multithread could not be obtained from the immediate context. NexVR issues its "
            "copy from the render thread and relies on that lock to serialise against the engine's "
            "own context use; without it, command streams interleave.";
        return NEXVR_ERROR_MULTITHREAD_UNAVAILABLE;
    }

    detail.clear();
    return NEXVR_SUCCESS;
}

NexVR_Result ValidateInitializeDX12(const RuntimeConstraints& constraints, const TextureFacts& target,
                                    NexVR_ColorSpace colorSpace, const LUID& deviceLuid,
                                    bool queueIsDirect, std::string& detail) {
    if (!constraints.queried) {
        detail =
            "NexVR_InitializeDX12 was called before NexVR_GetGraphicsRequirementsDX12. The adapter and "
            "render size are dictated by the runtime and are only knowable from that call, so it "
            "must run before the renderer creates its device.";
        return NEXVR_ERROR_REQUIREMENTS_NOT_QUERIED;
    }

    if (deviceLuid.LowPart != constraints.adapterLuid.LowPart ||
        deviceLuid.HighPart != constraints.adapterLuid.HighPart) {
        detail = std::format(
            "The D3D12 device is on adapter LUID {:08X}:{:08X} but the OpenXR runtime requires "
            "{:08X}:{:08X}. This cannot be corrected by copying — the render target lives on the "
            "wrong GPU. Create the device on the required adapter.",
            static_cast<uint32_t>(deviceLuid.HighPart), deviceLuid.LowPart,
            static_cast<uint32_t>(constraints.adapterLuid.HighPart), constraints.adapterLuid.LowPart);
        return NEXVR_ERROR_WRONG_ADAPTER;
    }

    if (!queueIsDirect) {
        detail =
            "The D3D12 command queue was null or not of type D3D12_COMMAND_LIST_TYPE_DIRECT. OpenXR "
            "requires a direct command queue for swapchain and copy operations.";
        return NEXVR_ERROR_INVALID_COMMAND_QUEUE;
    }

    if (!target.valid) {
        detail = "The representative render target could not be inspected.";
        return NEXVR_ERROR_INVALID_ARGUMENT;
    }

    if (target.width != constraints.requiredWidth || target.height != constraints.requiredHeight) {
        detail = std::format(
            "The render target is {}x{} but the runtime requires exactly {}x{}. CopyResource "
            "cannot rescale; a mismatch here is silently ignored and produces a session that "
            "submits frames containing nothing. Size the target from "
            "NexVR_GraphicsRequirements::requiredTextureWidth/Height.",
            target.width, target.height, constraints.requiredWidth, constraints.requiredHeight);
        return NEXVR_ERROR_DIMENSION_MISMATCH;
    }

    if (constraints.swapchainFormat != DXGI_FORMAT_UNKNOWN &&
        !CopyCompatible(target.format, constraints.swapchainFormat)) {
        detail = std::format(
            "Render target format {} cannot be copied into swapchain format {}: they do not share a "
            "typeless parent ({} vs {}). Note that R8G8B8A8 and B8G8R8A8 are different families "
            "despite both being 32-bit RGBA.",
            static_cast<int>(target.format), static_cast<int>(constraints.swapchainFormat),
            static_cast<int>(TypelessParent(target.format)),
            static_cast<int>(TypelessParent(constraints.swapchainFormat)));
        return NEXVR_ERROR_INVALID_FORMAT;
    }

    if (colorSpace == NEXVR_COLOR_SPACE_UNSPECIFIED) {
        detail =
            "colorSpace was not specified. There is no safe default: CopyResource performs no "
            "conversion, so bytes written as linear and interpreted as sRGB (or the reverse) "
            "produce a visibly wrong image while every call in the pipeline reports success. "
            "State NEXVR_COLOR_SPACE_SRGB_ENCODED or NEXVR_COLOR_SPACE_LINEAR.";
        return NEXVR_ERROR_COLOR_SPACE_UNSPECIFIED;
    }

    detail.clear();
    return NEXVR_SUCCESS;
}

bool IsDepthFormat(DXGI_FORMAT format) noexcept {
    switch (format) {
        case DXGI_FORMAT_D16_UNORM:
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
        case DXGI_FORMAT_D32_FLOAT:
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
            return true;
        // The typeless parents are accepted too. A game that creates its depth
        // buffer as R32_TYPELESS so it can also bind it as a shader resource —
        // which is how almost every deferred renderer reads depth back — has a
        // perfectly good depth buffer whose DXGI format never says "D".
        case DXGI_FORMAT_R16_TYPELESS:
        case DXGI_FORMAT_R24G8_TYPELESS:
        case DXGI_FORMAT_R32_TYPELESS:
        case DXGI_FORMAT_R32G8X24_TYPELESS:
            return true;
        default:
            return false;
    }
}

NexVR_Result ValidateDepthSubmit(uint32_t structSize, const TextureFacts& color,
                                 const TextureFacts& depth, float nearZ, float farZ,
                                 std::string& detail) {
    // Ordered so the first failure named is the one that has to be fixed first.
    // A caller with a stale header AND a wrong size should be told about the
    // header, because the size they are reading may not be the size they set.
    if (structSize < sizeof(NexVR_DepthInfoDX11)) {
        detail = std::format(
            "NexVR_DepthInfoDX11::structSize was {}, but this build of NexVR reads {} bytes. The "
            "struct was compiled against an older header and does not contain every field this "
            "runtime would read. Rebuild against the matching nexvr_sdk.h.",
            structSize, sizeof(NexVR_DepthInfoDX11));
        return NEXVR_ERROR_INVALID_ARGUMENT;
    }

    if (!depth.valid) {
        detail = "depthTexture was null, which should have been handled before validation.";
        return NEXVR_ERROR_INVALID_ARGUMENT;
    }

    if (!IsDepthFormat(depth.format)) {
        detail = std::format(
            "The submitted depth texture has format {}, which is not a depth format. Submitting a "
            "colour buffer here would produce a layer the runtime reprojects against meaningless "
            "values.",
            static_cast<int>(depth.format));
        return NEXVR_ERROR_INVALID_FORMAT;
    }

    if (depth.width != color.width || depth.height != color.height) {
        detail = std::format(
            "The depth texture is {}x{} and the colour texture is {}x{}. The runtime pairs them "
            "pixel for pixel, so a mismatch does not scale — it misreads.",
            depth.width, depth.height, color.width, color.height);
        return NEXVR_ERROR_DIMENSION_MISMATCH;
    }

    if (depth.sampleCount != 1) {
        // No resolve path exists, and inventing one is not a small change.
        // ResolveSubresource rejects depth formats outright, so resolving would
        // mean a full-screen shader pass with its own render target, its own
        // pipeline state to save and restore, and its own frame cost — none of
        // which belongs behind a validation function.
        detail = std::format(
            "The depth texture is multisampled ({} samples). CopyResource cannot copy a "
            "multisampled surface and ResolveSubresource does not accept depth formats, so NexVR "
            "cannot take a copy of it. Submit a resolved single-sample depth buffer, or omit depth.",
            depth.sampleCount);
        return NEXVR_ERROR_INVALID_ARGUMENT;
    }

    if (color.sampleCount != depth.sampleCount) {
        detail = "The colour and depth textures have different sample counts.";
        return NEXVR_ERROR_INVALID_ARGUMENT;
    }

    // --- The clip planes ---------------------------------------------------
    //
    // nearZ > farZ is NOT an error. That is reverse-Z, it is standard in modern
    // renderers, and XR_KHR_composition_layer_depth explicitly permits it. What
    // is checked is that both are usable numbers describing a real frustum.
    if (!std::isfinite(nearZ) || nearZ <= 0.0f) {
        detail = std::format(
            "nearZ was {}. The near plane distance must be a positive, finite number of metres; a "
            "zero or negative near plane does not describe a projection the runtime can invert.",
            nearZ);
        return NEXVR_ERROR_INVALID_ARGUMENT;
    }

    // Infinity is explicitly allowed here and nowhere else: an infinite
    // projection is a real technique, and it reports farZ as +inf rather than
    // as a large number.
    if (std::isnan(farZ) || farZ <= 0.0f) {
        detail = std::format(
            "farZ was {}. The far plane must be positive; INFINITY is accepted for an infinite "
            "projection.",
            farZ);
        return NEXVR_ERROR_INVALID_ARGUMENT;
    }

    if (nearZ == farZ) {
        detail = std::format(
            "nearZ and farZ are both {}. A frustum with no depth between its planes gives the "
            "runtime nothing to linearise.",
            nearZ);
        return NEXVR_ERROR_INVALID_ARGUMENT;
    }

    detail.clear();
    return NEXVR_SUCCESS;
}

NexVR_Result ValidateDepthSubmitDX12(uint32_t structSize, const TextureFacts& color,
                                     const TextureFacts& depth, float nearZ, float farZ,
                                     std::string& detail) {
    if (structSize < sizeof(NexVR_DepthInfoDX12)) {
        detail = std::format(
            "NexVR_DepthInfoDX12::structSize was {}, but this build of NexVR reads {} bytes. The "
            "struct was compiled against an older header and does not contain every field this "
            "runtime would read. Rebuild against the matching nexvr_sdk.h.",
            structSize, sizeof(NexVR_DepthInfoDX12));
        return NEXVR_ERROR_INVALID_ARGUMENT;
    }

    if (!depth.valid) {
        detail = "depthTexture was null, which should have been handled before validation.";
        return NEXVR_ERROR_INVALID_ARGUMENT;
    }

    if (!IsDepthFormat(depth.format)) {
        detail = std::format(
            "The submitted depth texture has format {}, which is not a depth format. Submitting a "
            "colour buffer here would produce a layer the runtime reprojects against meaningless "
            "values.",
            static_cast<int>(depth.format));
        return NEXVR_ERROR_INVALID_FORMAT;
    }

    if (depth.width != color.width || depth.height != color.height) {
        detail = std::format(
            "The depth texture is {}x{} and the colour texture is {}x{}. The runtime pairs them "
            "pixel for pixel, so a mismatch does not scale — it misreads.",
            depth.width, depth.height, color.width, color.height);
        return NEXVR_ERROR_DIMENSION_MISMATCH;
    }

    if (depth.sampleCount != 1) {
        detail = std::format(
            "The depth texture is multisampled ({} samples). CopyResource cannot copy a "
            "multisampled surface and ResolveSubresource does not accept depth formats, so NexVR "
            "cannot take a copy of it. Submit a resolved single-sample depth buffer, or omit depth.",
            depth.sampleCount);
        return NEXVR_ERROR_INVALID_ARGUMENT;
    }

    if (color.sampleCount != depth.sampleCount) {
        detail = "The colour and depth textures have different sample counts.";
        return NEXVR_ERROR_INVALID_ARGUMENT;
    }

    if (!std::isfinite(nearZ) || nearZ <= 0.0f) {
        detail = std::format(
            "nearZ was {}. The near plane distance must be a positive, finite number of metres; a "
            "zero or negative near plane does not describe a projection the runtime can invert.",
            nearZ);
        return NEXVR_ERROR_INVALID_ARGUMENT;
    }

    if (std::isnan(farZ) || farZ <= 0.0f) {
        detail = std::format(
            "farZ was {}. The far plane must be positive; INFINITY is accepted for an infinite "
            "projection.",
            farZ);
        return NEXVR_ERROR_INVALID_ARGUMENT;
    }

    if (nearZ == farZ) {
        detail = std::format(
            "nearZ and farZ are both {}. A frustum with no depth between its planes gives the "
            "runtime nothing to linearise.",
            nearZ);
        return NEXVR_ERROR_INVALID_ARGUMENT;
    }

    detail.clear();
    return NEXVR_SUCCESS;
}

NexVR_Result PublishFrameState(bool shouldRender, bool posesLocated, uint32_t viewCount,
                               const NexVR_View* views, NexVR_FrameState& out) noexcept {
    // Cleared FIRST, on every path, before anything can return.
    //
    // The caller owns this struct and is entitled to reuse it every frame. If a
    // skipped frame left viewCount alone, the value from the last successful
    // frame would still be sitting there next to poses that are now one frame
    // stale — and a game reading viewCount to decide whether to trust views[]
    // would be told yes. Zeroing costs nothing and removes the question.
    out.viewCount = 0;

    const bool haveViews = views != nullptr && viewCount > 0;
    if (!shouldRender || !posesLocated || !haveViews) {
        out.shouldRender = 0;
        return NEXVR_FRAME_SKIPPED;
    }

    // Clamped based on the caller's struct capacity.
    // If the caller passed an older header (structSize < sizeof(NexVR_FrameState)),
    // clamp to 2 views to prevent buffer overruns. If the caller allocated the
    // full 4-view struct, allow up to NEXVR_MAX_VIEWS (4).
    const uint32_t capacity = (out.structSize >= sizeof(NexVR_FrameState)) ? NEXVR_MAX_VIEWS : 2u;
    const uint32_t writable = viewCount > capacity ? capacity : viewCount;
    for (uint32_t eye = 0; eye < writable; ++eye) {
        out.views[eye] = views[eye];
    }

    out.viewCount = writable;
    out.shouldRender = 1;
    return NEXVR_SUCCESS;
}

NexVR_Result ValidateSubmit(bool shouldRender, const TextureFacts& submitted,
                            const RuntimeConstraints& constraints, std::string& detail) {
    if (!shouldRender) {
        // Nothing else is checked, because nothing else is read. The copy is
        // skipped entirely for a frame the runtime declined to display, so the
        // texture's size, format, and existence are all irrelevant — and
        // enforcing them anyway would be inventing a requirement the
        // implementation does not have.
        detail.clear();
        return NEXVR_SUCCESS;
    }

    if (!submitted.valid) {
        detail =
            "colorTexture was null on a frame that NexVR_WaitFrame reported as renderable. NULL is "
            "accepted only for a token whose WaitFrame did not return NEXVR_SUCCESS. This token is "
            "still outstanding; resubmit it with the render target.";
        return NEXVR_ERROR_INVALID_ARGUMENT;
    }

    // Re-checked every frame, not just at init. A studio with more than one
    // render target can hand us a different one on frame 900, and the failure
    // mode for that is silent — the whole reason this SDK exists.
    if (submitted.width != constraints.requiredWidth ||
        submitted.height != constraints.requiredHeight) {
        detail = std::format(
            "The submitted texture is {}x{}, not the {}x{} validated at initialization. The "
            "swapchain was sized once, from the runtime's requirement; a differently sized target "
            "cannot be copied into it and CopyResource would fail silently.",
            submitted.width, submitted.height, constraints.requiredWidth,
            constraints.requiredHeight);
        return NEXVR_ERROR_DIMENSION_MISMATCH;
    }

    detail.clear();
    return NEXVR_SUCCESS;
}

NexVR_Result ValidateControllerState(NexVR_Hand hand,
                                     const NexVR_ControllerState* state,
                                     std::string& detail) {
    if (hand != NEXVR_HAND_LEFT && hand != NEXVR_HAND_RIGHT) {
        detail = std::format(
            "hand was {}. Must be NEXVR_HAND_LEFT (0) or NEXVR_HAND_RIGHT (1).",
            static_cast<int>(hand));
        return NEXVR_ERROR_INVALID_ARGUMENT;
    }

    if (state == nullptr) {
        detail = "outState was null.";
        return NEXVR_ERROR_INVALID_ARGUMENT;
    }

    if (state->structSize < sizeof(NexVR_ControllerState)) {
        detail = std::format(
            "outState->structSize was {} bytes, smaller than sizeof(NexVR_ControllerState) ({} bytes). "
            "Rebuild against the matching nexvr_sdk.h or set structSize correctly.",
            state->structSize, sizeof(NexVR_ControllerState));
        return NEXVR_ERROR_INVALID_ARGUMENT;
    }

    detail.clear();
    return NEXVR_SUCCESS;
}

NexVR_Result ValidateHapticFeedback(NexVR_Hand hand,
                                    const NexVR_HapticFeedback* haptic,
                                    std::string& detail) {
    if (hand != NEXVR_HAND_LEFT && hand != NEXVR_HAND_RIGHT) {
        detail = std::format(
            "hand was {}. Must be NEXVR_HAND_LEFT (0) or NEXVR_HAND_RIGHT (1).",
            static_cast<int>(hand));
        return NEXVR_ERROR_INVALID_ARGUMENT;
    }

    if (haptic == nullptr) {
        detail = "haptic was null.";
        return NEXVR_ERROR_INVALID_ARGUMENT;
    }

    if (haptic->structSize < sizeof(NexVR_HapticFeedback)) {
        detail = std::format(
            "haptic->structSize was {} bytes, smaller than sizeof(NexVR_HapticFeedback) ({} bytes). "
            "Rebuild against the matching nexvr_sdk.h or set structSize correctly.",
            haptic->structSize, sizeof(NexVR_HapticFeedback));
        return NEXVR_ERROR_INVALID_ARGUMENT;
    }

    if (haptic->durationMs < 0.0f) {
        detail = std::format(
            "durationMs was {}. Duration must be non-negative (0.0 to stop haptics).",
            haptic->durationMs);
        return NEXVR_ERROR_INVALID_ARGUMENT;
    }

    if (haptic->amplitude < 0.0f || haptic->amplitude > 1.0f) {
        detail = std::format(
            "amplitude was {}. Amplitude must be normalized between 0.0 and 1.0.",
            haptic->amplitude);
        return NEXVR_ERROR_INVALID_ARGUMENT;
    }

    detail.clear();
    return NEXVR_SUCCESS;
}

}  // namespace nexvr::sdk

