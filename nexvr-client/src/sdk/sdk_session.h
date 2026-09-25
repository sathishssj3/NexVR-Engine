// The SDK's internal OpenXR + D3D11 core.
//
// ===========================================================================
// THIS TRANSLATION UNIT MUST NEVER CREATE A D3D11 DEVICE OR CONTEXT
// ===========================================================================
//
// It receives an ID3D11Device* the game already owns, and every D3D object it
// touches is either that device's or one the OpenXR runtime owns. If
// D3D11CreateDevice ever appears here, Model B has silently become Model A and
// the entire integration story changes.
//
// PROVENANCE: promoted verbatim from the Phase 0.9 spike
// (src/test_apps/b2b_dx11_prototype/nexvr_scope.*), not rewritten. Every
// safety property here was measured before it was shipped:
//
//   - requirements queried before the game has a device   (Phase 0.8 §4.1)
//   - ID3D11Multithread rather than a private mutex        (Phase 0.9)
//   - acquire and wait OUTSIDE the D3D lock                (Phase 0.9, Risk 1)
//   - a FINITE swapchain deadline and a survivable drop    (Phase 0.9)
//   - the verdict read from pixels, never from XrResult    (Phase 0.8 §4.6)
//
// It is deliberately NOT built from src/openxr/. That code is a process-wide
// singleton bound to the B2C injection lifecycle, and reshaping it into
// session-scoped state would be a rewrite of three working subsystems on the
// path the 30-day validation experiment depends on.

#pragma once

#include <d3d11.h>
// ID3D11Multithread is declared here, not in d3d11.h. It is the lock the game's
// renderer can also hold, which is why Phase 0.9 uses it rather than a private
// mutex — see the note on the m_multithread member.
#include <d3d11_4.h>
#include <d3d12.h>
#include <wrl/client.h>

#include <cstdint>
#include <string>
#include <vector>

// For NexVR_View, which FrameHandoff carries verbatim.
//
// This header is otherwise free of the public ABI, and that was worth keeping
// until it cost something. It did: the poses were located internally, converted
// by hand into the wrapper's struct, and the conversion silently omitted the
// only field that mattered. Two parallel pose layouts is one hand-copy per
// frame and one chance per field to drop it. NexVR_View is a POD of seven
// floats with no dependencies, so carrying it here removes the transcription
// step rather than moving it somewhere else.
#include "nexvr_sdk.h"

namespace nexvr::sdk {

using Microsoft::WRL::ComPtr;

/**
 * How far initialisation got.
 *
 * Ordered, and every value below Failed is a strictly greater amount of working
 * system than the one before it. On a machine with no headset the run stops at
 * a specific one of these, which is a data point rather than an error.
 */
enum class Stage {
    NotStarted,
    LoaderFound,
    InstanceCreated,
    SystemFound,
    GraphicsRequirementsQueried,
    SessionCreated,
    SwapchainsCreated,
    SessionRunning,
    FrameSubmitted,
};

[[nodiscard]] const char* StageName(Stage stage) noexcept;

enum class GraphicsApi {
    D3D11,
    D3D12,
};

/** One OpenXR call and what it answered. */
struct CallRecord {
    std::string call;
    std::string result;
    bool succeeded{};
};

/** Wall-clock cost of the pieces section 5 of the results asks about. */
struct FrameTimings {
    double waitFrameMs{};
    double copyMs{};
    double acquireWaitMs{};
    double endFrameMs{};
    /** Time spent waiting to enter the D3D11 immediate-context lock. */
    double lockWaitMsInternal{};
};

/**
 * What the runtime demands of the game BEFORE the game has a device.
 *
 * All of this is answerable from an instance and a system id alone — no
 * session, and therefore no device. That ordering is the single most important
 * integration finding of this prototype: the runtime dictates both the adapter
 * and the render target size, and a game that has already created its device
 * and sized its targets cannot be retrofitted by copying.
 */
struct Requirements {
    LUID adapterLuid{};
    uint32_t eyeWidth{};
    uint32_t eyeHeight{};
    uint32_t viewCount{};
};

/**
 * The outcome of one frame, including whether the copy ACTUALLY happened.
 *
 * copyVerified exists because of a false positive this prototype produced on
 * its first run: every OpenXR call returned XR_SUCCESS, thirty frames were
 * accepted by the compositor, and not one pixel was copied, because
 * CopyResource had rejected a size mismatch and CopyResource returns void.
 *
 * A frame loop that reports success from "no XrResult failed" is measuring the
 * wrong thing. This field is the readback that measures the right one.
 */
struct FrameResult {
    FrameTimings timings;
    bool verifyAttempted{false};
    bool copyVerified{false};
    uint8_t sampledPixel[4]{};

    /** The compositor did not free an image in time; this frame was dropped. */
    bool droppedOnTimeout{false};
    /** How long the D3D11 immediate-context lock was contended. */
    double lockWaitMs{0.0};
};

/**
 * One frame's predicted state, produced on the simulation thread and consumed
 * on the render thread.
 *
 * The token is what makes the handoff checkable. OpenXR requires strict 1:1
 * pairing between its frame-wait and frame-begin, and with those calls on
 * different threads nothing else would catch a violated pairing before the
 * runtime hit undefined behaviour.
 */
struct FrameHandoff {
    uint64_t token{};
    int64_t predictedDisplayTime{};
    bool shouldRender{false};
    uint32_t viewCount{};

    /**
     * Per-eye pose and FOV, located once for predictedDisplayTime.
     *
     * Located HERE, on the simulation thread, and cached against the token so
     * that the composition layer built at submit time uses these exact values
     * rather than locating again.
     *
     * That is not a micro-optimisation, it is the correctness property. The
     * compositor reprojects the submitted pixels against whatever pose the
     * layer names. Locating a second time at submit produces a pose ~one frame
     * newer than the one the game rendered from, so the reprojection would
     * correct for head motion that the pixels already contain — a small,
     * constant, direction-dependent error that looks like the world sliding
     * under the head, and that no return code anywhere reports.
     *
     * The cost is that the pose is as old as the wait, roughly 11.8 ms on
     * SteamVR at 84 Hz. That is what predictedDisplayTime is for: it is a
     * PREDICTION for display time, not a sample of now, so waiting longer to
     * ask does not make it more correct — it only makes it disagree with the
     * frame it belongs to.
     */
    NexVR_View views[NEXVR_MAX_VIEWS]{};

    /**
     * Whether xrLocateViews returned usable positions for this frame.
     *
     * Separate from viewCount because the runtime can answer XR_SUCCESS with
     * the position bit clear — tracking dropped, headset off the head,
     * recentring in progress. The views array is populated in that case, with
     * values that are not where the head is. Treated as a skipped frame rather
     * than rendered from, because a frame drawn from an unlocatable pose is
     * indistinguishable from a correct one in every log the SDK produces.
     */
    bool posesValid{false};
};

class SdkSession {
public:
    SdkSession() = default;
    ~SdkSession();

    SdkSession(const SdkSession&) = delete;
    SdkSession& operator=(const SdkSession&) = delete;

    /**
     * Queries the runtime for the adapter it requires, WITHOUT a device.
     *
     * Split out from Initialize because of an ordering constraint that is easy
     * to miss and expensive to discover late: the runtime dictates which GPU
     * the D3D11 device must live on, and it will only tell you after
     * xrCreateInstance. A game that has already created its device on another
     * adapter cannot be bound, and no amount of copying fixes that — the
     * texture is on the wrong GPU.
     *
     * So a real Model B integration has to run this BEFORE the game's device
     * exists, which means NexVR initialisation cannot simply be bolted on after
     * engine startup. That is an integration-order finding, not an API detail.
     */
    bool QueryRequirements(Requirements& requirements, std::string& error);

    /** Binds to the game's existing device and brings up session + swapchains. */
    /**
     * Create the session and swapchain against an already-bound device.
     *
     * @param targetFormat  The game target's format. The swapchain format is
     *                      chosen from the runtime's offered list PREFERRING one
     *                      this can be copied into, rather than picking a
     *                      favourite and rejecting the game afterwards.
     */
    bool Initialize(ID3D11Device* gameDevice, DXGI_FORMAT targetFormat, std::string& error);

    /**
     * Pumps the OpenXR event queue and drives the session state machine.
     *
     * @return false once the runtime has told us to stop.
     */
    bool PumpEvents(std::string& error);

    /** True once the runtime has moved the session to a frame-submitting state. */
    [[nodiscard]] bool ReadyToSubmit() const noexcept { return m_sessionRunning; }

    /**
     * One full Model B frame: wait, begin, acquire, COPY, release, end.
     *
     * @param gameTexture  The game's render target. Not owned, not modified.
     * @param verifyCopy   Read one pixel back out of the swapchain image before
     *                     releasing it. This forces a CPU/GPU sync and must not
     *                     be left on for a timing run — see the results
     *                     document, where the cost is measured rather than
     *                     asserted.
     */
    bool SubmitFrame(ID3D11Texture2D* gameTexture, bool verifyCopy, FrameResult& result,
                     std::string& error);

    /**
     * Wait for the runtime to release the next frame. SIMULATION THREAD.
     *
     * Wraps xrWaitFrame and nothing else — no D3D, no swapchain, no lock. That
     * is what makes it safe to call from a thread that has never heard of the
     * immediate context.
     */
    bool WaitFrame(FrameHandoff& handoff, std::string& error);

    /**
     * Copy and submit a frame the simulation thread waited for. RENDER THREAD.
     *
     * The ordering inside is the entire point of Phase 0.9:
     *
     *   1. xrAcquireSwapchainImage      no D3D lock held
     *   2. xrWaitSwapchainImage         no D3D lock held, FINITE timeout
     *   3. ID3D11Multithread::Enter()
     *   4. CopyResource
     *   5. ID3D11Multithread::Leave()
     *   6. xrReleaseSwapchainImage, xrEndFrame
     *
     * Steps 1 and 2 block on the compositor. Holding the D3D11 lock across
     * them would mean a compositor hiccup freezes every thread in the engine
     * that needs the context — including the one that would run the panic
     * teardown. A dropped frame is a flicker; a held lock is a reboot.
     */
    /**
     * One frame's depth buffer and what its values mean.
     *
     * Passed by pointer and nullable, so the colour-only path stays exactly the
     * path it was — not a special case of a depth path, which would put new code
     * between every existing caller and the compositor.
     */
    struct DepthSubmission {
        ID3D11Texture2D* texture{nullptr};
        float nearZ{};
        float farZ{};
        /** True when the near plane sits at 1.0 in the buffer (reverse-Z). */
        bool reversed{false};
    };

    bool SubmitFrameThreaded(uint64_t token, ID3D11Texture2D* gameTexture, bool verifyCopy,
                             FrameResult& result, std::string& error,
                             const DepthSubmission* depth = nullptr);

    struct DepthSubmissionDX12 {
        ID3D12Resource* texture{nullptr};
        float nearZ{};
        float farZ{};
        bool reversed{false};
        D3D12_RESOURCE_STATES state{D3D12_RESOURCE_STATE_DEPTH_WRITE};
    };

    bool SubmitFrameThreadedDX12(uint64_t token, ID3D12Resource* gameTexture,
                                 D3D12_RESOURCE_STATES colorState, bool verifyCopy,
                                 FrameResult& result, std::string& error,
                                 const DepthSubmissionDX12* depth = nullptr);

    /**
     * Whether an outstanding token's frame is one the game rendered.
     *
     * A read, not a redemption — the token stays outstanding. Exists so the ABI
     * layer can decide which argument checks apply before it commits to the
     * submission: a texture is required for a rendered frame and irrelevant for
     * a skipped one, and that question cannot be answered from the arguments
     * alone.
     *
     * @return false if the token is not outstanding, in which case
     *         `shouldRender` is untouched. The caller does not need to
     *         distinguish that case — SubmitFrameThreaded rejects the same
     *         token with a message that names it — so this returns a plain bool
     *         rather than a second error channel saying the same thing twice.
     */
    [[nodiscard]] bool TokenShouldRender(uint64_t token, bool& shouldRender) const;

    /**
     * Override the swapchain-image wait deadline. Default is half a frame.
     *
     * Exists so the timeout path can be forced rather than waited for. A
     * recovery path that has never executed is a recovery path nobody has
     * tested.
     */
    void SetSwapchainWaitTimeoutNanos(int64_t nanos) noexcept;

    /**
     * Treat every Nth swapchain wait as though it had timed out. 0 disables.
     *
     * Fault injection, because the real thing could not be provoked. Setting
     * the deadline to 1 ns produced ZERO timeouts on this machine: with no
     * headset the compositor never holds an image, so the wait always returns
     * immediately and the recovery path never ran.
     *
     * A recovery path that has never executed is indistinguishable from one
     * that does not work, so this forces the branch and lets the spike answer
     * the question that actually matters — does the session survive, does the
     * render thread keep making progress, and is the still-acquired image
     * picked up again next frame.
     */
    void SetSimulatedTimeoutEvery(uint32_t everyNthFrame) noexcept;

    /**
     * Report every Nth frame as one the runtime does not want rendered.
     * 0 disables.
     *
     * The second fault injector, added for the same reason as the first and
     * discovered the same way. A game meets skipped frames constantly — the
     * headset goes on a desk, the runtime recentres, the compositor throttles —
     * and the development machine could not produce one: SteamVR with no HMD
     * attached asked for ninety frames out of ninety.
     *
     * So the branch where a game submits a token with NO TEXTURE, which is the
     * whole of the SDK-02 contract, was covered by unit tests and had never once
     * run against a real runtime. This is what makes it run.
     */
    void SetSimulatedSkipEvery(uint32_t everyNthFrame) noexcept;

    [[nodiscard]] uint32_t FramesDropped() const noexcept { return m_framesDropped; }
    [[nodiscard]] bool MultithreadProtected() const noexcept { return m_multithreadProtected; }
    [[nodiscard]] bool MultithreadWasAlreadyOn() const noexcept { return m_multithreadWasOn; }

    /**
     * Bind to the game's device WITHOUT creating the session yet.
     *
     * Split out of Initialize so that validation runs against a real device and
     * a real texture before any OpenXR object exists. Failing validation must
     * leave the runtime exactly as it was — a rejected integration attempt that
     * had already created a session would leave a studio's next attempt
     * fighting a session they cannot see.
     */
    bool BindDevice(ID3D11Device* gameDevice, ID3D11DeviceContext* gameContext,
                    std::string& error);

    /** True once BindDevice has a context and it is the immediate one. */
    [[nodiscard]] bool ContextIsImmediate() const noexcept;

    /** The LUID of the adapter the bound device actually lives on. */
    [[nodiscard]] bool DeviceAdapterLuid(LUID& luid) const noexcept;

    /** The swapchain format chosen from what the runtime offered. */
    [[nodiscard]] DXGI_FORMAT ChosenSwapchainFormat() const noexcept {
        return static_cast<DXGI_FORMAT>(m_format);
    }

    /**
     * True when Initialize failed specifically because no format the runtime
     * offers can be copied into from the game's target.
     *
     * A distinct signal rather than a generic failure, because it is the one
     * initialization error a studio can fix by changing one line of their
     * renderer, and it deserves to say so.
     */
    [[nodiscard]] bool FailedOnFormat() const noexcept { return m_failedOnFormat; }

    void Shutdown();

    [[nodiscard]] Stage ReachedStage() const noexcept { return m_stage; }
    [[nodiscard]] const std::vector<CallRecord>& Calls() const noexcept { return m_calls; }
    [[nodiscard]] const std::string& RuntimeName() const noexcept { return m_runtimeName; }
    [[nodiscard]] uint32_t SwapchainWidth() const noexcept { return m_width; }
    [[nodiscard]] uint32_t SwapchainHeight() const noexcept { return m_height; }
    [[nodiscard]] int64_t SwapchainFormat() const noexcept { return m_format; }
    [[nodiscard]] uint32_t ViewCount() const noexcept { return m_viewCount; }

    /** Whether submitted depth will actually reach the compositor. */
    [[nodiscard]] bool DepthUsable() const noexcept { return m_depthUsable; }
    [[nodiscard]] DXGI_FORMAT DepthFormat() const noexcept {
        return static_cast<DXGI_FORMAT>(m_depthFormat);
    }
    [[nodiscard]] uint32_t FramesSubmitted() const noexcept { return m_framesSubmitted; }

    /** The runtime-owned textures backing the swapchain, for inspection. */
    [[nodiscard]] const std::vector<ID3D11Texture2D*>& SwapchainImages() const noexcept {
        return m_swapchainImages;
    }

    bool QueryRequirementsDX12(Requirements& requirements, std::string& error);
    bool BindDeviceDX12(ID3D12Device* gameDevice, ID3D12CommandQueue* gameQueue, std::string& error);
    [[nodiscard]] bool QueueIsDirect() const noexcept;
    [[nodiscard]] bool DeviceAdapterLuidDX12(LUID& luid) const noexcept;
    bool InitializeDX12(ID3D12Device* gameDevice, ID3D12CommandQueue* gameQueue, DXGI_FORMAT targetFormat, std::string& error);
    [[nodiscard]] GraphicsApi ActiveApi() const noexcept { return m_api; }
    [[nodiscard]] const std::vector<ID3D12Resource*>& SwapchainImagesDX12() const noexcept {
        return m_d3d12SwapchainImages;
    }

    // --- 6DOF Controller Input & Haptics ---------------------------------
    bool InitializeInput(std::string& error);
    bool SyncInput(std::string& error);
    bool GetControllerState(NexVR_Hand hand, NexVR_ControllerState& outState, std::string& error);
    bool TriggerHaptic(NexVR_Hand hand, const NexVR_HapticFeedback& haptic, std::string& error);
    bool StopHaptic(NexVR_Hand hand, std::string& error);

private:
    GraphicsApi m_api{GraphicsApi::D3D11};
    ComPtr<ID3D12Device> m_d3d12Device;
    ComPtr<ID3D12CommandQueue> m_d3d12Queue;
    ComPtr<ID3D12CommandAllocator> m_d3d12CommandAllocator;
    ComPtr<ID3D12GraphicsCommandList> m_d3d12CommandList;
    ComPtr<ID3D12Fence> m_d3d12Fence;
    HANDLE m_d3d12FenceEvent{nullptr};
    uint64_t m_d3d12FenceValue{0};

    std::vector<ID3D12Resource*> m_d3d12SwapchainImages;
    std::vector<ID3D12Resource*> m_d3d12DepthSwapchainImages;
    // Deliberately opaque. Keeping the OpenXR handles out of this header means
    // main.cpp does not need XR_USE_GRAPHICS_API_D3D11 defined, and the game
    // side could not accidentally reach an XrSession even if it tried.
    struct Impl;
    Impl* m_impl{nullptr};

    // NOT owned. This is the game's device and the game's context; NexVR holds
    // a reference so they cannot vanish underneath a frame in flight, and drops
    // it in Shutdown. Section 3 of the results is about exactly this reference.
    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_context;

    /**
     * The engine's own lock on the immediate context, not one of ours.
     *
     * A private std::mutex here would protect NexVR from NexVR and nothing
     * else. ID3D11Multithread is the only lock the game's renderer can also be
     * holding, which is the whole reason it is the right one.
     */
    ComPtr<ID3D11Multithread> m_multithread;
    bool m_multithreadProtected{false};
    bool m_failedOnFormat{false};
    bool m_multithreadWasOn{false};
    uint32_t m_framesDropped{0};

    std::vector<ID3D11Texture2D*> m_swapchainImages;
    std::vector<CallRecord> m_calls;
    std::string m_runtimeName;

    Stage m_stage{Stage::NotStarted};
    uint32_t m_width{};
    uint32_t m_height{};
    int64_t m_format{};
    uint32_t m_viewCount{};

    // --- Depth (optional throughout) ------------------------------------
    // Three separate flags because they answer three different questions, and
    // collapsing them would make a diagnostic say the wrong thing: the runtime
    // may implement the extension but offer no usable depth format, and the
    // swapchain may exist while a particular frame supplies no depth.
    bool m_depthExtensionAvailable{false};
    bool m_depthUsable{false};
    int64_t m_depthFormat{};
    std::vector<ID3D11Texture2D*> m_depthSwapchainImages;
    uint32_t m_framesSubmitted{};
    bool m_sessionRunning{false};
};

}  // namespace nexvr::sdk
