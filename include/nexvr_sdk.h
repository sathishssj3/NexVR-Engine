/*===========================================================================
  nexvr_sdk.h — NexVR B2B SDK, Phase 1 draft ABI
  ---------------------------------------------------------------------------
  DESIGN PREMISE

  Phase 0.8 established that a Model B integration can be completely broken
  while every API in the stack reports success: CopyResource returns void, so a
  dimension or format mismatch is a silent no-op, and the OpenXR session around
  it happily accepts 300 frames of nothing. See docs/B2B_DX11_PROTOTYPE_RESULTS
  section 4.6.

  Every design choice below exists to make one of those silent failures loud:

    - Requirements are queried BEFORE the game has a device, because the
      runtime dictates the adapter and the render size and neither can be
      renegotiated afterwards.
    - Initialization takes the game's actual texture and validates it, rather
      than trusting a description of it.
    - Colour space is a REQUIRED field with no default, because the copy does
      not convert and nothing downstream can detect a wrong answer.
    - Every error is a distinct code naming the specific violated constraint.

  ABI STABILITY

  C only. No C++ types, no exceptions, no ownership transfer across the
  boundary. Every struct carries `structSize` as its first member so fields can
  be appended without breaking a caller compiled against an older header.
===========================================================================*/

#ifndef NEXVR_SDK_H
#define NEXVR_SDK_H

#include <stdint.h>

#if defined(_WIN32)
#  include <d3d11.h>
#  include <d3d12.h>
#else
#  error "NexVR SDK is Windows-only. Target games are Win32/x64."
#endif

#if !defined(NEXVR_NO_VULKAN)
#  if defined(__has_include)
#    if __has_include(<vulkan/vulkan.h>)
#      ifndef VULKAN_H_
#        include <vulkan/vulkan.h>
#      endif
#      define NEXVR_HAS_VULKAN 1
#    endif
#  elif defined(VK_VERSION_1_0) || defined(VULKAN_H_)
#    define NEXVR_HAS_VULKAN 1
#  endif
#endif

#ifndef NEXVR_HAS_VULKAN
typedef struct VkInstance_T*       VkInstance;
typedef struct VkPhysicalDevice_T* VkPhysicalDevice;
typedef struct VkDevice_T*         VkDevice;
typedef struct VkQueue_T*          VkQueue;
typedef uint64_t                   VkImage;
typedef uint64_t                   VkSemaphore;
typedef int32_t                    VkImageLayout;
typedef int32_t                    VkFormat;
#define NEXVR_HAS_VULKAN 1
#define NEXVR_VULKAN_FALLBACK_TYPES 1
#endif

#define NEXVR_SDK_VERSION_MAJOR 0
#define NEXVR_SDK_VERSION_MINOR 3
#define NEXVR_SDK_VERSION_PATCH 0

#define NEXVR_MAKE_VERSION(ma, mi, pa) (((ma) << 22) | ((mi) << 12) | (pa))
#define NEXVR_SDK_VERSION \
    NEXVR_MAKE_VERSION(NEXVR_SDK_VERSION_MAJOR, NEXVR_SDK_VERSION_MINOR, NEXVR_SDK_VERSION_PATCH)

#ifdef NEXVR_SDK_IMPLEMENTATION
#  define NEXVR_API __declspec(dllexport)
#else
#  define NEXVR_API __declspec(dllimport)
#endif

/* Explicit and never inherited from the caller's project settings. A studio
   building with /Gz would otherwise silently corrupt the stack. */
#define NEXVR_CALL __stdcall

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
  Results
===========================================================================*/

/**
 * @brief Outcome of every SDK call.
 *
 * Deliberately granular. "Initialization failed" is the answer that generates a
 * support ticket; NEXVR_ERROR_DIMENSION_MISMATCH is the answer that resolves
 * one. Each code below names exactly one violated precondition.
 *
 * Negative values are errors, zero is success, positive values are non-fatal
 * conditions the caller may reasonably continue through.
 */
typedef enum NexVR_Result {
    NEXVR_SUCCESS = 0,

    /** The runtime asked us to stop. Tear down and return to flat rendering. */
    NEXVR_SESSION_EXITING = 1,
    /** This frame should not be rendered. Skip it; do not treat as an error. */
    NEXVR_FRAME_SKIPPED = 2,

    /* --- Startup ordering ------------------------------------------------ */
    NEXVR_ERROR_RUNTIME_UNAVAILABLE   = -1,  /**< No OpenXR runtime installed. */
    NEXVR_ERROR_NO_HEADSET            = -2,  /**< Runtime present, no system.  */
    NEXVR_ERROR_REQUIREMENTS_NOT_QUERIED = -3,
        /**< InitializeDX11 called before GetGraphicsRequirements. The adapter
             and size constraints are only knowable from that call, so this
             ordering is not a style preference. */

    /* --- The four Phase 0.8 constraints, one code each -------------------- */
    NEXVR_ERROR_WRONG_ADAPTER         = -10,
        /**< The device is not on the LUID the runtime requires. Not
             recoverable by copying: the texture is on the wrong GPU. */
    NEXVR_ERROR_DIMENSION_MISMATCH    = -11,
        /**< Texture dimensions differ from NexVR_GraphicsRequirements. In
             Phase 0.8 this produced 30 accepted frames containing no image. */
    NEXVR_ERROR_INVALID_FORMAT        = -12,
        /**< Format has no typeless parent in common with the swapchain.
             R8G8B8A8 -> B8G8R8A8 lands here, and BGRA is what most DXGI
             swapchains use. */
    NEXVR_ERROR_NOT_IMMEDIATE_CONTEXT = -13,
        /**< A deferred context was supplied. Its command list carries no
             ordering against the game's draws, so the copy may execute before
             the frame it is copying — a stale image that reads as latency. */
    NEXVR_ERROR_INVALID_COMMAND_QUEUE = -14,
        /**< The D3D12 command queue was null or not of type DIRECT. OpenXR
             requires a direct command queue for swapchain and copy operations. */
    NEXVR_ERROR_INVALID_VULKAN_QUEUE  = -15,
        /**< The Vulkan queue was null or its queue family index does not support graphics/transfer. */

    /* --- Contract violations the caller must resolve --------------------- */
    NEXVR_ERROR_COLOR_SPACE_UNSPECIFIED = -20,
        /**< colorSpace was left NEXVR_COLOR_SPACE_UNSPECIFIED. There is no
             safe default: the copy does not convert, so guessing produces an
             image that is wrong in a way no API reports. */
    NEXVR_ERROR_MULTITHREAD_UNAVAILABLE = -21,
        /**< ID3D11Multithread could not be obtained or enabled. See the
             threading contract on NexVR_SubmitFrameDX11. */

    /* --- Frame lifecycle -------------------------------------------------- */
    NEXVR_ERROR_INVALID_FRAME_TOKEN   = -30,
        /**< The token was already submitted, never issued, or is stale.
             Enforces the 1:1 WaitFrame/SubmitFrame pairing the runtime
             requires. */
    NEXVR_ERROR_SWAPCHAIN_TIMEOUT     = -31,
        /**< The compositor did not release an image within the deadline. The
             frame is dropped rather than blocking the render thread. */

    /* --- Generic ---------------------------------------------------------- */
    NEXVR_ERROR_INVALID_ARGUMENT      = -40,
    NEXVR_ERROR_INVALID_SESSION       = -41,
    NEXVR_ERROR_RUNTIME_FAILURE       = -42,
} NexVR_Result;

#define NEXVR_SUCCEEDED(r) ((r) >= 0)
#define NEXVR_FAILED(r)    ((r) < 0)

/*===========================================================================
  Colour space — no default, deliberately
===========================================================================*/

/**
 * @brief How the game's render target encodes colour.
 *
 * This field has no sensible default and the SDK refuses to start without it.
 *
 * Phase 0.8 measured the reason: copying R8G8B8A8_UNORM into
 * R8G8B8A8_UNORM_SRGB is legal and performs NO conversion. Byte (17,59,101)
 * arrives as (17,59,101), and the compositor then linearises it as though it
 * were sRGB-encoded. If the game wrote linear values, the displayed image is
 * wrong — while every call returns success and every frame is accepted.
 *
 * Making the studio state this converts a silent wrongness into a contract.
 */
typedef enum NexVR_ColorSpace {
    NEXVR_COLOR_SPACE_UNSPECIFIED = 0,  /**< Rejected at init. */
    NEXVR_COLOR_SPACE_SRGB_ENCODED = 1,
        /**< Gamma already applied; bytes are display-ready. Most engines'
             final post-processing output. */
    NEXVR_COLOR_SPACE_LINEAR = 2,
        /**< Raw linear values. NexVR must convert; a plain CopyResource
             would be incorrect. */
} NexVR_ColorSpace;

/*===========================================================================
  Handles and structures
===========================================================================*/

typedef struct NexVR_Session_T* NexVR_Session;

/**
 * @brief What the runtime demands before the game creates its device.
 *
 * Every field here constrains an object the game builds during renderer
 * startup, which is why this must be obtainable without a device.
 */
typedef struct NexVR_GraphicsRequirements {
    uint32_t structSize;      /**< sizeof(NexVR_GraphicsRequirements). */

    LUID     adapterLuid;     /**< The device MUST be created on this adapter. */
    uint32_t minFeatureLevel; /**< D3D_FEATURE_LEVEL as a raw value. */

    uint32_t recommendedWidth;   /**< Per eye. */
    uint32_t recommendedHeight;  /**< Per eye. */
    uint32_t maxWidth;
    uint32_t maxHeight;
    uint32_t viewCount;          /**< 2 for stereo. */

    /**
     * The exact texture size the game must provide to NexVR_SubmitFrameDX11,
     * already accounting for viewCount and the submission layout.
     *
     * Precomputed rather than left as arithmetic on the fields above, because
     * an integrator who multiplies wrong gets NEXVR_ERROR_DIMENSION_MISMATCH
     * at init instead of a black headset at runtime.
     */
    uint32_t requiredTextureWidth;
    uint32_t requiredTextureHeight;

    /** DXGI_FORMAT the swapchain will use. The game's target must share this
        format's typeless parent. */
    uint32_t swapchainFormat;
} NexVR_GraphicsRequirements;

/**
 * @brief Everything NexVR needs to bind to an already-running renderer.
 */
typedef struct NexVR_InitializeDX11Info {
    uint32_t structSize;

    /** Borrowed, never owned. NexVR holds a reference for its lifetime. */
    ID3D11Device*        device;

    /** MUST be the immediate context. Validated via GetType(). */
    ID3D11DeviceContext* immediateContext;

    /**
     * A representative render target, used for validation only — never read.
     *
     * Passing the real texture rather than a description is deliberate. A
     * description can be filled in wrongly and still look plausible; the
     * texture cannot disagree with itself.
     */
    ID3D11Texture2D*     representativeTarget;

    NexVR_ColorSpace     colorSpace;  /**< Required. See NexVR_ColorSpace. */

    /**
     * When non-zero, NexVR reads back a pixel after each copy and reports a
     * mismatch instead of assuming the copy occurred.
     *
     * Costs a full CPU/GPU sync per frame. Intended for integration bring-up,
     * not shipping. It exists because Phase 0.8's own harness reported PASS
     * across 30 frames that contained nothing.
     */
    uint32_t             enableCopyVerification;

    /**
     * View configuration topology. Defaults to NEXVR_VIEW_CONFIGURATION_STEREO (2 views).
     * Set to NEXVR_VIEW_CONFIGURATION_QUAD_VIEWS for 4-view foveated rendering.
     */
    uint32_t             viewConfiguration;
} NexVR_InitializeDX11Info;

/**
 * @brief Everything NexVR needs to bind to an already-running D3D12 renderer.
 */
typedef struct NexVR_InitializeDX12Info {
    uint32_t             structSize;

    /** Borrowed, never owned. NexVR holds a reference for its lifetime. */
    ID3D12Device*        device;

    /** MUST be a DIRECT command queue (D3D12_COMMAND_LIST_TYPE_DIRECT). */
    ID3D12CommandQueue*  commandQueue;

    /**
     * A representative render target, used for validation only — never read.
     *
     * Passing the real resource rather than a description is deliberate. A
     * description can be filled in wrongly and still look plausible; the
     * resource cannot disagree with itself.
     */
    ID3D12Resource*      representativeTarget;

    NexVR_ColorSpace     colorSpace;  /**< Required. See NexVR_ColorSpace. */

    /**
     * When non-zero, NexVR reads back a pixel after each copy and reports a
     * mismatch instead of assuming the copy occurred.
     */
    uint32_t             enableCopyVerification;

    /**
     * View configuration topology. Defaults to NEXVR_VIEW_CONFIGURATION_STEREO (2 views).
     * Set to NEXVR_VIEW_CONFIGURATION_QUAD_VIEWS for 4-view foveated rendering.
     */
    uint32_t             viewConfiguration;
} NexVR_InitializeDX12Info;

/**
 * @brief What the runtime demands for a Vulkan renderer before device creation.
 */
typedef struct NexVR_GraphicsRequirementsVK {
    uint32_t structSize;            /**< sizeof(NexVR_GraphicsRequirementsVK). */
    uint32_t minApiVersionSupported;/**< Minimum Vulkan API version (VK_MAKE_VERSION). */
    uint32_t maxApiVersionSupported;/**< Maximum Vulkan API version (VK_MAKE_VERSION). */
    uint64_t physicalDevice;        /**< VkPhysicalDevice handle or pointer. */

    uint32_t recommendedWidth;      /**< Per eye recommended width. */
    uint32_t recommendedHeight;     /**< Per eye recommended height. */
    uint32_t maxWidth;
    uint32_t maxHeight;
    uint32_t viewCount;             /**< Number of views (e.g. 2 for stereo). */

    uint32_t requiredTextureWidth;  /**< recommendedWidth * viewCount. */
    uint32_t requiredTextureHeight; /**< recommendedHeight. */

    uint32_t swapchainFormat;       /**< Preferred VkFormat for color swapchain. */
} NexVR_GraphicsRequirementsVK;

/**
 * @brief Everything NexVR needs to bind to an already-running Vulkan renderer.
 */
typedef struct NexVR_InitializeVKInfo {
    uint32_t         structSize;            /**< sizeof(NexVR_InitializeVKInfo). */

    VkInstance       instance;              /**< The game's active VkInstance. */
    VkPhysicalDevice physicalDevice;        /**< The game's active VkPhysicalDevice. */
    VkDevice         device;                /**< The game's active VkDevice. */
    uint32_t         queueFamilyIndex;      /**< Queue family index for graphics/transfer. */
    uint32_t         queueIndex;            /**< Queue index within the family (usually 0). */
    VkQueue          queue;                 /**< Active VkQueue with graphics/transfer capability. */

    VkImage          representativeTarget;  /**< Representative render target VkImage for validation. */
    uint32_t         representativeFormat;  /**< VkFormat of the representative target. */
    uint32_t         representativeWidth;   /**< Width of representative target in pixels. */
    uint32_t         representativeHeight;  /**< Height of representative target in pixels. */

    NexVR_ColorSpace colorSpace;            /**< Required. See NexVR_ColorSpace. */
    uint32_t         enableCopyVerification;/**< Verification flag. */
    uint32_t         viewConfiguration;     /**< NexVR_ViewConfiguration. */
} NexVR_InitializeVKInfo;

/**
 * @brief Which end of the depth range is near.
 *
 * Reverse-Z is not an exotic option to be tolerated; it is standard in modern
 * DX12-era renderers, because spending floating-point precision where depth
 * resolution actually matters is worth the swapped comparison. NexVR's own
 * projection validator already declines to assume the textbook convention, and
 * this enum is the caller-facing half of that: a game is asked which convention
 * it uses rather than having one inferred from its numbers.
 */
typedef enum NexVR_DepthRange {
    /** Near plane at 0.0, far plane at 1.0. The textbook arrangement. */
    NEXVR_DEPTH_RANGE_ZERO_TO_ONE = 0,

    /** Near plane at 1.0, far plane at 0.0. Reverse-Z. */
    NEXVR_DEPTH_RANGE_REVERSED = 1,
} NexVR_DepthRange;

/**
 * @brief Vulkan depth buffer information for frame submission.
 */
typedef struct NexVR_DepthInfoVK {
    uint32_t          structSize;           /**< sizeof(NexVR_DepthInfoVK). */
    VkImage           depthImage;           /**< Native depth VkImage handle. */
    uint32_t          depthFormat;          /**< VkFormat (e.g. VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT). */
    VkImageLayout     currentLayout;        /**< Current layout of depthImage before NexVR reads it. */
    NexVR_DepthRange  depthRange;           /**< Standard or Reversed Z. */
    float             nearZ;                /**< Near clipping plane in meters. */
    float             farZ;                 /**< Far clipping plane in meters. */
} NexVR_DepthInfoVK;

#define NEXVR_MAX_VIEWS 4

/**
 * @brief View configuration topologies supported by NexVR.
 */
typedef enum NexVR_ViewConfiguration {
    NEXVR_VIEW_CONFIGURATION_STEREO = 1,           /**< Standard 2-view stereoscopic VR (default). */
    NEXVR_VIEW_CONFIGURATION_QUAD_VIEWS = 2,       /**< 4-view foveated rendering (Varjo / Pimax / XR_VARJO_quad_views). */
    NEXVR_VIEW_CONFIGURATION_STEREO_SPECTATOR = 3, /**< 3-view stereo + broadcast spectator camera. */
} NexVR_ViewConfiguration;

/** Minimal pose data for the simulation thread. */
typedef struct NexVR_Pose {
    float orientation[4];  /**< Quaternion x, y, z, w. */
    float position[3];     /**< Metres. */
} NexVR_Pose;

typedef struct NexVR_View {
    NexVR_Pose pose;
    float      fovAngles[4];  /**< Radians: left, right, up, down. */
} NexVR_View;

/**
 * @brief One frame's predicted state, produced by NexVR_WaitFrame.
 *
 * The `token` is what ties a WaitFrame on one thread to a SubmitFrame on
 * another. OpenXR requires strict 1:1 pairing between its frame-wait and
 * frame-begin calls; carrying an opaque token makes a violated pairing an
 * explicit NEXVR_ERROR_INVALID_FRAME_TOKEN rather than undefined behaviour
 * inside the runtime.
 *
 * @par The one invariant that matters
 * `NexVR_WaitFrame` returned NEXVR_SUCCESS
 *   <=> `shouldRender` is 1
 *   <=> `viewCount` is non-zero
 *   <=> `views[]` holds poses located for `predictedDisplayTime`.
 *
 * All four move together, always. On any other return code `viewCount` is 0
 * and `views[]` is NOT written — whatever the caller left in the struct stays
 * there. Read poses only after a NEXVR_SUCCESS.
 *
 * This is stricter than it needs to be, deliberately. The runtime can report a
 * frame worth rendering while tracking is momentarily unlocatable, and the
 * honest thing to hand back for that frame is "do not render" rather than a
 * pose that is merely the last known one wearing the same struct. A game that
 * renders from a stale pose produces a session where every call succeeds and
 * the world does not track the head — the single worst failure this SDK exists
 * to prevent, because nothing in the return codes can detect it afterwards.
 */
typedef struct NexVR_FrameState {
    uint32_t   structSize;

    uint64_t   token;              /**< Opaque. Pass to NexVR_SubmitFrameDX11. */
    int64_t    predictedDisplayTime;
    uint32_t   shouldRender;       /**< 0 means skip rendering this frame. */

    /**
     * Number of entries written to views[]. Zero unless this call returned
     * NEXVR_SUCCESS. Read this rather than assuming 2 — it is the only field
     * that distinguishes "populated" from "left as you passed it in".
     */
    uint32_t   viewCount;

    /**
     * Predicted per-eye pose and field of view, valid for
     * `predictedDisplayTime`, expressed in the session's reference space.
     *
     * These are the SAME values NexVR hands the compositor when the matching
     * NexVR_SubmitFrameDX11 runs. They are located once, on this call, and
     * cached against the token. Locating again at submit time would give the
     * compositor a fresher pose than the one the game actually rendered from,
     * and the compositor reprojects against what it is told — so a fresher
     * number there is not an improvement, it is a mismatch with the pixels.
     *
     * Up to NEXVR_MAX_VIEWS (4 for quad-view foveated rendering).
     */
    NexVR_View views[NEXVR_MAX_VIEWS];
} NexVR_FrameState;

/*===========================================================================
  6DOF Controller Input & Haptics
===========================================================================*/

/**
 * @brief Identifies left or right VR motion controller hand.
 */
typedef enum NexVR_Hand {
    NEXVR_HAND_LEFT  = 0,
    NEXVR_HAND_RIGHT = 1,
    NEXVR_HAND_COUNT = 2
} NexVR_Hand;

/* --- Controller Button Bitmasks (NexVR_ControllerState::buttonsDown) ------ */
#define NEXVR_BUTTON_PRIMARY         (1u << 0)  /**< A on right touch, X on left touch. */
#define NEXVR_BUTTON_SECONDARY       (1u << 1)  /**< B on right touch, Y on left touch. */
#define NEXVR_BUTTON_THUMBSTICK      (1u << 2)  /**< Thumbstick / trackpad click. */
#define NEXVR_BUTTON_MENU            (1u << 3)  /**< System / Menu / Application menu. */
#define NEXVR_BUTTON_TRIGGER_CLICK   (1u << 4)  /**< Trigger pull past click threshold. */
#define NEXVR_BUTTON_GRIP_CLICK      (1u << 5)  /**< Grip / Squeeze click. */
#define NEXVR_BUTTON_SHOULDER        (1u << 6)  /**< Bumper / shoulder if mapped. */

/* --- Controller Touch Sensor Bitmasks (NexVR_ControllerState::buttonsTouched) */
#define NEXVR_TOUCH_PRIMARY          (1u << 0)  /**< Capacitive touch on primary button. */
#define NEXVR_TOUCH_SECONDARY        (1u << 1)  /**< Capacitive touch on secondary button. */
#define NEXVR_TOUCH_THUMBSTICK       (1u << 2)  /**< Capacitive touch on thumbstick/trackpad. */
#define NEXVR_TOUCH_TRIGGER          (1u << 3)  /**< Capacitive touch on trigger index curl. */
#define NEXVR_TOUCH_THUMBREST        (1u << 4)  /**< Capacitive touch on thumbrest surface. */

/**
 * @brief 6DOF tracking and input state for one VR motion controller.
 *
 * @par ABI stability
 * `structSize` must be set to `sizeof(NexVR_ControllerState)` before passing
 * to NexVR_GetControllerState.
 */
typedef struct NexVR_ControllerState {
    uint32_t    structSize;        /**< sizeof(NexVR_ControllerState). */
    uint32_t    isConnected;       /**< 1 if controller is active and detected, 0 otherwise. */
    uint32_t    gripPoseValid;     /**< 1 if gripPose position/orientation are tracked and valid. */
    uint32_t    aimPoseValid;      /**< 1 if aimPose position/orientation are tracked and valid. */

    NexVR_Pose  gripPose;          /**< 6DOF pose where the user grips the controller. */
    NexVR_Pose  aimPose;           /**< 6DOF ray/aim pose pointing from the controller front. */

    float       trigger;           /**< Analog trigger index finger pull [0.0, 1.0]. */
    float       grip;              /**< Analog grip / squeeze value [0.0, 1.0]. */
    float       thumbstickX;       /**< Horizontal thumbstick / trackpad [-1.0, 1.0]. */
    float       thumbstickY;       /**< Vertical thumbstick / trackpad [-1.0, 1.0]. */

    uint32_t    buttonsDown;       /**< Bitmask of NEXVR_BUTTON_* currently pressed. */
    uint32_t    buttonsTouched;    /**< Bitmask of NEXVR_TOUCH_* currently touched. */

    float       linearVelocity[3];  /**< Velocity in m/s in reference space, if tracked. */
    float       angularVelocity[3]; /**< Angular velocity in rad/s, if tracked. */
} NexVR_ControllerState;

/**
 * @brief Haptic vibration request parameters.
 *
 * @par ABI stability
 * `structSize` must be set to `sizeof(NexVR_HapticFeedback)`.
 */
typedef struct NexVR_HapticFeedback {
    uint32_t structSize;           /**< sizeof(NexVR_HapticFeedback). */
    float    durationMs;           /**< Vibration duration in milliseconds (e.g. 50.0f). 0 to stop. */
    float    frequencyHz;          /**< Frequency in Hertz (0.0 for runtime default, typically ~160Hz). */
    float    amplitude;            /**< Normalized vibration amplitude [0.0, 1.0]. */
} NexVR_HapticFeedback;

/*===========================================================================
  Lifecycle
===========================================================================*/

/**
 * @brief Query what the runtime requires, BEFORE creating a D3D11 device.
 *
 * @par Why this exists
 * The runtime dictates both the adapter and the render target size, and only
 * reveals them after its own instance exists. A game that has already created
 * its device on another adapter cannot be bound — the texture is on the wrong
 * GPU and no amount of copying reaches it. A game that has already sized its
 * targets will fail every copy silently.
 *
 * This inverts normal renderer startup and that is the point. Calling it after
 * the renderer boots is the single most likely integration mistake.
 *
 * @par Threading
 * Call from any single thread during startup. Not safe to call concurrently
 * with itself. Requires no session and no device.
 *
 * @param[out] requirements  Caller-allocated; set structSize before calling.
 *
 * @retval NEXVR_SUCCESS
 * @retval NEXVR_ERROR_RUNTIME_UNAVAILABLE  No OpenXR runtime is installed.
 * @retval NEXVR_ERROR_NO_HEADSET           No system available.
 *
 * @warning A successful return does NOT prove a display is attached. Phase 0.8
 *          measured SteamVR/OpenXR 2.16.7 returning a valid system with no HMD
 *          present, then accepting 300+ submitted frames. Do not use this as a
 *          hardware-presence check.
 */
NEXVR_API NexVR_Result NEXVR_CALL
NexVR_GetGraphicsRequirements(NexVR_GraphicsRequirements* requirements);

/**
 * @brief Bind NexVR to the game's existing device and validate every contract.
 *
 * @par What is validated, and what each failure means
 * - Device adapter LUID vs the required one .... NEXVR_ERROR_WRONG_ADAPTER
 * - Target dimensions vs requiredTexture* ...... NEXVR_ERROR_DIMENSION_MISMATCH
 * - Format's typeless parent vs swapchain ...... NEXVR_ERROR_INVALID_FORMAT
 * - Context type is immediate .................. NEXVR_ERROR_NOT_IMMEDIATE_CONTEXT
 * - colorSpace was stated ...................... NEXVR_ERROR_COLOR_SPACE_UNSPECIFIED
 * - ID3D11Multithread obtainable ............... NEXVR_ERROR_MULTITHREAD_UNAVAILABLE
 *
 * Every one of these is checked HERE, once, loudly. All of them are otherwise
 * discovered as a black headset with no error anywhere in the log.
 *
 * @par Multithread protection
 * NexVR calls ID3D11Multithread::SetMultithreadProtected(TRUE) on the device.
 * If the engine relies on the immediate context being unsynchronised for
 * performance, this is a behaviour change it must be told about — it is
 * reported through NexVR_GetLastErrorDetail as an informational note.
 *
 * @par Threading
 * Call once, from the thread that owns the immediate context. Not safe to call
 * concurrently with any other SDK function.
 */
NEXVR_API NexVR_Result NEXVR_CALL
NexVR_InitializeDX11(const NexVR_InitializeDX11Info* info, NexVR_Session* outSession);

/**
 * @brief Block until the runtime says to begin the next frame; return poses.
 *
 * @par Threading — READ THIS
 * Designed to be called from the GAME/SIMULATION thread, on a different thread
 * from NexVR_SubmitFrameDX11. The OpenXR specification explicitly supports
 * calling its frame-wait from a thread other than the one performing frame
 * submission; this is the documented pipelined-engine pattern.
 *
 * Constraints that ARE the caller's responsibility:
 * - Calls to this function must be serialised with each other. Two simulation
 *   threads calling it concurrently is undefined.
 * - Every returned token must be passed to exactly one NexVR_SubmitFrameDX11,
 *   in the order issued.
 *
 * @warning This call BLOCKS for the runtime's frame interval (~11.8 ms measured
 *          on SteamVR at 84 Hz). It is the runtime's throttle. If the render
 *          thread stops submitting, this will eventually stall the simulation
 *          thread — see the risk assessment on pipeline decoupling.
 *
 * @retval NEXVR_FRAME_SKIPPED     shouldRender is 0. Still submit the token.
 * @retval NEXVR_SESSION_EXITING   Tear down; do not submit.
 */
NEXVR_API NexVR_Result NEXVR_CALL
NexVR_WaitFrame(NexVR_Session session, NexVR_FrameState* outFrame);

/**
 * @brief Copy the game's finished render target into the runtime's swapchain
 *        and submit the frame to the compositor.
 *
 * @par Threading — READ THIS
 * MUST be called from the thread that owns the immediate context — in Unreal
 * and Unity, the Render Thread. This is not advisory:
 *
 * 1. The copy is issued on the immediate context so that D3D11's command
 *    ordering places it after the game's own draws for this frame. A deferred
 *    context records an independent command list with no such ordering, and
 *    the copy could execute against a partially-drawn or previous frame.
 * 2. ID3D11DeviceContext is not thread-safe. NexVR takes the
 *    ID3D11Multithread lock around its own context use, which protects against
 *    interleaving ONLY if the engine also participates in that lock.
 *
 * @par What this function does NOT hold a lock across
 * The wait for a free swapchain image happens BEFORE the context lock is
 * taken. Blocking on the compositor while holding the D3D11 lock would convert
 * a compositor hiccup into a whole-engine deadlock. See the risk assessment.
 *
 * @param token         From a NexVR_FrameState. Consumed; single use.
 * @param colorTexture  The game's finished render target. Read, never written,
 *                      never retained past this call.
 *
 *                      MAY BE NULL, and only when the NexVR_WaitFrame that
 *                      issued this token did NOT return NEXVR_SUCCESS. Such a
 *                      frame still has to be submitted — OpenXR ends every
 *                      frame it begins, and a token quietly dropped instead
 *                      leaves the runtime's frame open and desynchronises the
 *                      wait/begin pairing that the rest of this API depends on.
 *
 *                      This exists so that a skipped frame costs the game
 *                      nothing. Requiring a texture here would mean keeping a
 *                      correctly-sized render target alive purely to satisfy an
 *                      argument that is never read, which is a strange thing to
 *                      demand of an engine that was just told not to render.
 *
 *                      Passing NULL on a frame that DID return NEXVR_SUCCESS is
 *                      a caller bug and returns NEXVR_ERROR_INVALID_ARGUMENT;
 *                      the token stays outstanding so the call can be retried
 *                      with the real texture.
 *
 * @retval NEXVR_ERROR_INVALID_FRAME_TOKEN  Token reused, stale, or out of order.
 * @retval NEXVR_ERROR_SWAPCHAIN_TIMEOUT    Compositor did not free an image in
 *                                          time. The frame was DROPPED. This is
 *                                          survivable — do not tear down.
 * @retval NEXVR_ERROR_DIMENSION_MISMATCH   Only when enableCopyVerification is
 *                                          set, or the texture differs from the
 *                                          one validated at init.
 */
NEXVR_API NexVR_Result NEXVR_CALL
NexVR_SubmitFrameDX11(NexVR_Session session, uint64_t token, ID3D11Texture2D* colorTexture);

/*===========================================================================
  Depth submission (optional)
===========================================================================*/

/**
 * @brief The game's depth buffer for one frame, and what its values mean.
 *
 * Submitting depth lets the runtime reproject more accurately and improves
 * occlusion against runtime-drawn content. It is entirely optional: a game that
 * passes nothing here gets exactly the colour-only projection layer it got
 * before this struct existed, through exactly the same code path.
 *
 * @par structSize is the compatibility mechanism
 * Set it to `sizeof(NexVR_DepthInfoDX11)` as your headers define it. The SDK
 * reads only the fields your value says are present, so a game built against an
 * older header keeps working against a newer runtime, and a game built against
 * a newer header is refused politely by an older one rather than reading past
 * the end of a struct it does not understand.
 */
typedef struct NexVR_DepthInfoDX11 {
    uint32_t structSize;

    /**
     * The depth texture for this frame.
     *
     * Must match the colour texture's width, height, and sample count, and must
     * be a depth format sharing a typeless parent with one the runtime offers.
     * Not multisampled: CopyResource cannot copy a multisampled depth surface
     * and ResolveSubresource does not accept depth formats, so a resolved copy
     * would have to be a shader pass this SDK does not perform.
     *
     * NULL is legal and means "no depth this frame" — see
     * NexVR_SubmitFrameWithDepthDX11.
     */
    ID3D11Texture2D* depthTexture;

    /**
     * Distance to the near and far clip planes, in metres, both positive.
     *
     * These describe the projection the game rendered with, and the runtime
     * uses them to linearise the buffer. Getting them wrong does not fail:
     * it produces reprojection that is subtly wrong in a way only a headset
     * reveals, which is why they are validated rather than trusted.
     *
     * farZ may be INFINITY for an infinite projection. nearZ may be greater
     * than farZ — that is what reverse-Z looks like, and it is accepted.
     */
    float nearZ;
    float farZ;

    /** Which end of the buffer's value range is the near plane. */
    NexVR_DepthRange range;
} NexVR_DepthInfoDX11;

/**
 * @brief The game's D3D12 depth buffer for one frame, and what its values mean.
 */
typedef struct NexVR_DepthInfoDX12 {
    uint32_t             structSize;

    /**
     * The depth texture for this frame.
     *
     * Must match the colour texture's width, height, and sample count (1).
     */
    ID3D12Resource*      depthTexture;

    /** Distance to the near and far clip planes, in metres, both positive. */
    float                nearZ;
    float                farZ;

    /** Which end of the buffer's value range is the near plane. */
    NexVR_DepthRange     range;

    /**
     * The resource state of depthTexture when passed into SubmitFrame.
     * NexVR transitions depthTexture from this state to D3D12_RESOURCE_STATE_COPY_SOURCE
     * and restores it before submission completes.
     */
    D3D12_RESOURCE_STATES depthState;
} NexVR_DepthInfoDX12;

/**
 * @brief Query what the runtime requires, BEFORE creating a D3D12 device.
 */
NEXVR_API NexVR_Result NEXVR_CALL
NexVR_GetGraphicsRequirementsDX12(NexVR_GraphicsRequirements* requirements);

/**
 * @brief Bind NexVR to the game's existing D3D12 device and direct command queue.
 */
NEXVR_API NexVR_Result NEXVR_CALL
NexVR_InitializeDX12(const NexVR_InitializeDX12Info* info, NexVR_Session* outSession);

/**
 * @brief Submit a D3D12 frame.
 */
NEXVR_API NexVR_Result NEXVR_CALL
NexVR_SubmitFrameDX12(NexVR_Session session, uint64_t token,
                      ID3D12Resource* colorTexture,
                      D3D12_RESOURCE_STATES colorState);

/**
 * @brief Submit a D3D12 frame, optionally with its depth buffer.
 */
NEXVR_API NexVR_Result NEXVR_CALL
NexVR_SubmitFrameWithDepthDX12(NexVR_Session session, uint64_t token,
                               ID3D12Resource* colorTexture,
                               D3D12_RESOURCE_STATES colorState,
                               const NexVR_DepthInfoDX12* depth);

/**
 * @brief Submit a D3D12 frame with explicit asynchronous GPU fence synchronization.
 *
 * Allows the game engine to schedule VR submission without CPU stalls. NexVR commands
 * its direct queue to wait on waitFence prior to copying, and signals signalFence when done.
 */
NEXVR_API NexVR_Result NEXVR_CALL
NexVR_SubmitFrameDX12Ex(NexVR_Session session, uint64_t token,
                        ID3D12Resource* colorTexture,
                        D3D12_RESOURCE_STATES colorState,
                        ID3D12Fence* waitFence, uint64_t waitFenceValue,
                        ID3D12Fence* signalFence, uint64_t signalFenceValue);

/**
 * @brief Submit a D3D12 frame with depth and explicit asynchronous GPU fence synchronization.
 */
NEXVR_API NexVR_Result NEXVR_CALL
NexVR_SubmitFrameWithDepthDX12Ex(NexVR_Session session, uint64_t token,
                                 ID3D12Resource* colorTexture,
                                 D3D12_RESOURCE_STATES colorState,
                                 const NexVR_DepthInfoDX12* depth,
                                 ID3D12Fence* waitFence, uint64_t waitFenceValue,
                                 ID3D12Fence* signalFence, uint64_t signalFenceValue);

/**
 * @brief Submit a frame, optionally with its depth buffer.
 *
 * Identical to NexVR_SubmitFrameDX11 in every respect — same threading rules,
 * same token contract, same NULL-colour allowance for a skipped frame — except
 * that it also accepts depth.
 *
 * @par Why a new function rather than a new parameter
 * NexVR_SubmitFrameDX11 is already exported and already called. Changing its
 * signature would break every binary linked against it, and a studio's build is
 * not ours to invalidate for a feature they did not ask for. The old entry
 * point remains, unchanged, and forwards here with no depth.
 *
 * @param depth  May be NULL, and NULL is not a degraded mode: the frame is
 *               submitted as a plain colour projection layer, byte for byte the
 *               same submission the colour-only path produces.
 *
 *               Ignored, with no error, when the runtime does not support
 *               XR_KHR_composition_layer_depth. A game should not have to
 *               branch on runtime capabilities to render normally, and failing
 *               a frame over an unavailable optional extension would turn a
 *               quality improvement into a compatibility requirement.
 *               NexVR_SupportsDepthSubmission reports whether it is being used.
 *
 * @retval NEXVR_ERROR_DIMENSION_MISMATCH   Depth is not the same size as colour.
 * @retval NEXVR_ERROR_INVALID_ARGUMENT     structSize is too small, the clip
 *                                          planes are not usable, or the
 *                                          texture is multisampled.
 * @retval NEXVR_ERROR_INVALID_FORMAT       The depth format cannot be copied
 *                                          into anything the runtime offers.
 */
NEXVR_API NexVR_Result NEXVR_CALL
NexVR_SubmitFrameWithDepthDX11(NexVR_Session session, uint64_t token,
                               ID3D11Texture2D* colorTexture,
                               const NexVR_DepthInfoDX11* depth);

/**
 * @brief Whether this session can actually use submitted depth.
 *
 * False when the runtime does not implement XR_KHR_composition_layer_depth, or
 * when it offers no depth format the game's buffer can be copied into. A game
 * may use this to skip preparing a depth copy it knows will be discarded; it
 * does not need to consult it merely to call the function safely.
 */
NEXVR_API uint32_t NEXVR_CALL
NexVR_SupportsDepthSubmission(NexVR_Session session);

/*===========================================================================
  Vulkan Native Path Functions
===========================================================================*/

/**
 * @brief Query what the runtime requires before creating a Vulkan device.
 */
NEXVR_API NexVR_Result NEXVR_CALL
NexVR_GetGraphicsRequirementsVK(NexVR_GraphicsRequirementsVK* requirements);

/**
 * @brief Bind NexVR to the game's existing Vulkan instance, physical device, device, and queue.
 */
NEXVR_API NexVR_Result NEXVR_CALL
NexVR_InitializeVK(const NexVR_InitializeVKInfo* info, NexVR_Session* outSession);

/**
 * @brief Submit a Vulkan frame with optional semaphore synchronization.
 */
NEXVR_API NexVR_Result NEXVR_CALL
NexVR_SubmitFrameVK(NexVR_Session session, uint64_t frameToken,
                    VkImage colorImage, VkImageLayout currentLayout,
                    VkSemaphore waitSemaphore, VkSemaphore signalSemaphore);

/**
 * @brief Submit a Vulkan frame with depth and optional semaphore synchronization.
 */
NEXVR_API NexVR_Result NEXVR_CALL
NexVR_SubmitFrameWithDepthVK(NexVR_Session session, uint64_t frameToken,
                            VkImage colorImage, VkImageLayout currentColorLayout,
                            const NexVR_DepthInfoVK* depth,
                            VkSemaphore waitSemaphore, VkSemaphore signalSemaphore);

/*===========================================================================
  6DOF Controller Input & Haptics Functions
===========================================================================*/

/**
 * @brief Synchronize input actions with the OpenXR runtime.
 *
 * Typically called once per frame on the simulation or game thread before polling
 * controller states. Can also be called at higher frequencies if the engine has
 * a dedicated input tick.
 *
 * @param session Active NexVR session.
 * @retval NEXVR_SUCCESS
 * @retval NEXVR_ERROR_INVALID_SESSION
 * @retval NEXVR_ERROR_RUNTIME_FAILURE
 */
NEXVR_API NexVR_Result NEXVR_CALL
NexVR_SyncInput(NexVR_Session session);

/**
 * @brief Retrieve the 6DOF tracking and input state for a controller.
 *
 * @param session  Active NexVR session.
 * @param hand     NEXVR_HAND_LEFT or NEXVR_HAND_RIGHT.
 * @param outState Pointer to caller-allocated NexVR_ControllerState with structSize set.
 * @retval NEXVR_SUCCESS
 * @retval NEXVR_ERROR_INVALID_ARGUMENT
 * @retval NEXVR_ERROR_INVALID_SESSION
 */
NEXVR_API NexVR_Result NEXVR_CALL
NexVR_GetControllerState(NexVR_Session session, NexVR_Hand hand, NexVR_ControllerState* outState);

/**
 * @brief Trigger haptic vibration on a controller.
 *
 * @param session Active NexVR session.
 * @param hand    NEXVR_HAND_LEFT or NEXVR_HAND_RIGHT.
 * @param haptic  Pointer to NexVR_HapticFeedback structure (structSize must be set).
 *                If haptic is NULL or amplitude is 0, halts active haptics on that hand.
 * @retval NEXVR_SUCCESS
 * @retval NEXVR_ERROR_INVALID_ARGUMENT
 * @retval NEXVR_ERROR_INVALID_SESSION
 */
NEXVR_API NexVR_Result NEXVR_CALL
NexVR_TriggerHaptic(NexVR_Session session, NexVR_Hand hand, const NexVR_HapticFeedback* haptic);

/**
 * @brief Immediately stop any active haptic vibration on a controller.
 *
 * @param session Active NexVR session.
 * @param hand    NEXVR_HAND_LEFT or NEXVR_HAND_RIGHT.
 * @retval NEXVR_SUCCESS
 * @retval NEXVR_ERROR_INVALID_ARGUMENT
 * @retval NEXVR_ERROR_INVALID_SESSION
 */
NEXVR_API NexVR_Result NEXVR_CALL
NexVR_StopHaptic(NexVR_Session session, NexVR_Hand hand);

/**
 * @brief Shut down. Safe to call from any thread, but not concurrently with
 *        WaitFrame or SubmitFrameDX11 — join those first.
 *
 * Releases NexVR's reference to the game's device LAST, after the OpenXR
 * session is destroyed. The reverse order asks the runtime to clean up a
 * session whose graphics binding has already been dropped.
 */
NEXVR_API void NEXVR_CALL
NexVR_Shutdown(NexVR_Session session);

/**
 * @brief Human-readable detail for the most recent failure on this thread.
 *
 * Thread-local. Valid until the next SDK call on the same thread. Returns a
 * static empty string rather than NULL when there is nothing to report.
 */
NEXVR_API const char* NEXVR_CALL
NexVR_GetLastErrorDetail(void);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* NEXVR_SDK_H */
