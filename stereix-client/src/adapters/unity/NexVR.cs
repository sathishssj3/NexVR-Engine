// P/Invoke declarations for the NexVR B2B SDK and its Unity bridge.
//
// ============================================================================
// SCOPE: DECLARATIONS ONLY
// ============================================================================
//
// There is deliberately no MonoBehaviour here, no CommandBuffer, and no camera
// integration. Those decisions belong to the studio's render pipeline — a Built-in
// pipeline, URP, and HRP each hook the frame differently — and shipping one
// opinionated MonoBehaviour would mean two of the three studios have to fight it.
//
// What this file guarantees is that the marshalling is right, which is the part a
// studio should not have to get right themselves. See
// docs/B2B_UNITY_INTEGRATION_PLAN.md for how these calls are meant to be sequenced.

using System;
using System.Runtime.InteropServices;

namespace NexVR
{
    /// <summary>Outcome of every SDK call. Mirrors NexVR_Result in nexvr_sdk.h.</summary>
    /// <remarks>
    /// Negative is an error, zero is success, positive is a non-fatal condition the
    /// caller may continue through. Check with <see cref="Result.Failed"/> rather
    /// than <c>!= Success</c>, or a dropped frame reads as a fatal error.
    /// </remarks>
    public enum NexVRResult
    {
        Success = 0,

        SessionExiting = 1,
        FrameSkipped = 2,

        ErrorRuntimeUnavailable = -1,
        ErrorNoHeadset = -2,
        ErrorRequirementsNotQueried = -3,

        ErrorWrongAdapter = -10,
        ErrorDimensionMismatch = -11,
        ErrorInvalidFormat = -12,
        ErrorNotImmediateContext = -13,

        ErrorColorSpaceUnspecified = -20,
        ErrorMultithreadUnavailable = -21,

        ErrorInvalidFrameToken = -30,
        ErrorSwapchainTimeout = -31,

        ErrorInvalidArgument = -40,
        ErrorInvalidSession = -41,
        ErrorRuntimeFailure = -42,
    }

    /// <summary>
    /// How the render target encodes colour. There is no default and the SDK
    /// refuses to start without one.
    /// </summary>
    /// <remarks>
    /// This is not a preference. The copy into the OpenXR swapchain performs NO
    /// conversion, so bytes written as linear and interpreted as sRGB produce a
    /// visibly wrong image while every call in the pipeline reports success.
    ///
    /// In Unity this maps directly onto <c>QualitySettings.activeColorSpace</c>,
    /// which is the one thing a project has already been forced to decide.
    /// </remarks>
    public enum NexVRColorSpace
    {
        Unspecified = 0,
        SrgbEncoded = 1,
        Linear = 2,
    }

    /// <summary>Which end of the depth range is near. Mirrors NexVR_DepthRange in nexvr_sdk.h.</summary>
    public enum NexVRDepthRange
    {
        ZeroToOne = 0,
        Reversed = 1,
    }

    /// <summary>Controller hand designation. Mirrors NexVR_Hand in nexvr_sdk.h.</summary>
    public enum NexVRHand
    {
        Left = 0,
        Right = 1,
    }

    /// <summary>Controller digital button bitmasks. Mirrors NEXVR_BUTTON_* in nexvr_sdk.h.</summary>
    public static class NexVRButton
    {
        public const uint Trigger = 1 << 0;
        public const uint Grip = 1 << 1;
        public const uint Primary = 1 << 2;    // A on Right, X on Left
        public const uint Secondary = 1 << 3;  // B on Right, Y on Left
        public const uint Thumbstick = 1 << 4;
        public const uint Menu = 1 << 5;
    }

    /// <summary>Controller capacitive touch bitmasks. Mirrors NEXVR_TOUCH_* in nexvr_sdk.h.</summary>
    public static class NexVRTouch
    {
        public const uint Trigger = 1 << 0;
        public const uint Primary = 1 << 1;
        public const uint Secondary = 1 << 2;
        public const uint Thumbstick = 1 << 3;
        public const uint Thumbrest = 1 << 4;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Vector2f
    {
        public float X, Y;

        public Vector2f(float x, float y)
        {
            X = x;
            Y = y;
        }

        public override string ToString() => $"({X:F3}, {Y:F3})";
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Vector3f
    {
        public float X, Y, Z;

        public Vector3f(float x, float y, float z)
        {
            X = x;
            Y = y;
            Z = z;
        }

        public override string ToString() => $"({X:F3}, {Y:F3}, {Z:F3})";
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Quaternionf
    {
        public float X, Y, Z, W;

        public Quaternionf(float x, float y, float z, float w)
        {
            X = x;
            Y = y;
            Z = z;
            W = w;
        }

        public override string ToString() => $"({X:F3}, {Y:F3}, {Z:F3}, {W:F3})";
    }

    /// <summary>Vibration haptic feedback command. Mirrors NexVR_HapticFeedback in nexvr_sdk.h.</summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct HapticFeedback
    {
        public uint StructSize;
        public float DurationMs;
        public float FrequencyHz;
        public float Amplitude;

        public static HapticFeedback Create(float durationMs, float amplitude, float frequencyHz = 0.0f)
        {
            return new HapticFeedback
            {
                StructSize = (uint)Marshal.SizeOf<HapticFeedback>(),
                DurationMs = durationMs,
                FrequencyHz = frequencyHz,
                Amplitude = amplitude
            };
        }

        public static HapticFeedback FromSeconds(float durationSeconds, float amplitude, float frequencyHz = 0.0f)
        {
            return Create(durationSeconds * 1000.0f, amplitude, frequencyHz);
        }
    }

    /// <summary>View configuration topologies supported by NexVR.</summary>
    public enum NexVRViewConfiguration
    {
        Stereo = 1,
        QuadViews = 2,
        StereoSpectator = 3,
    }

    /// <summary>Optional depth buffer metadata for submission. Mirrors NexVR_DepthInfoDX11 in nexvr_sdk.h.</summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct DepthInfoDX11
    {
        public uint StructSize;
        public IntPtr DepthTexture;
        public float NearZ;
        public float FarZ;
        public NexVRDepthRange Range;
    }

    /// <summary>Optional depth buffer metadata for D3D12 submission. Mirrors NexVR_DepthInfoDX12 in nexvr_sdk.h.</summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct DepthInfoDX12
    {
        public uint StructSize;
        public IntPtr DepthTexture;
        public float NearZ;
        public float FarZ;
        public NexVRDepthRange Range;
        public uint DepthState;
    }

    /// <summary>What the runtime requires BEFORE Unity's device exists.</summary>
    /// <remarks>
    /// <see cref="RequiredTextureWidth"/> and <see cref="RequiredTextureHeight"/>
    /// are precomputed. Do not derive them from the per-eye fields — getting that
    /// multiplication wrong is the exact mistake that cost Phase 0.8 thirty frames
    /// of silently empty submissions.
    /// </remarks>
    [StructLayout(LayoutKind.Sequential)]
    public struct GraphicsRequirements
    {
        public uint StructSize;

        public uint AdapterLuidLow;
        public int AdapterLuidHigh;

        public uint MinFeatureLevel;

        public uint RecommendedWidth;
        public uint RecommendedHeight;
        public uint MaxWidth;
        public uint MaxHeight;
        public uint ViewCount;

        public uint RequiredTextureWidth;
        public uint RequiredTextureHeight;

        public uint SwapchainFormat;
    }

    /// <summary>Arguments to <see cref="Native.InitializeDX11"/>.</summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct InitializeDX11Info
    {
        public uint StructSize;

        public IntPtr Device;
        public IntPtr ImmediateContext;
        public IntPtr RepresentativeTarget;

        public NexVRColorSpace ColorSpace;

        /// <summary>
        /// Non-zero makes the SDK read a pixel back after each copy and fail
        /// rather than assume the copy happened. Costs a full CPU/GPU sync per
        /// frame — for integration bring-up, never for a shipping build.
        /// </summary>
        public uint EnableCopyVerification;

        /// <summary>
        /// View configuration topology (Stereo = 1, QuadViews = 2).
        /// </summary>
        public uint ViewConfiguration;
    }

    /// <summary>Arguments to <see cref="Native.InitializeDX12"/>.</summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct InitializeDX12Info
    {
        public uint StructSize;

        public IntPtr Device;
        public IntPtr CommandQueue;
        public IntPtr RepresentativeTarget;

        public NexVRColorSpace ColorSpace;
        public uint EnableCopyVerification;
        public uint ViewConfiguration;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Pose
    {
        public float OrientationX, OrientationY, OrientationZ, OrientationW;
        public float PositionX, PositionY, PositionZ;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct View
    {
        public Pose Pose;
        public float FovLeft, FovRight, FovUp, FovDown;
    }

    /// <summary>One frame's predicted state, from <see cref="Native.WaitFrame"/>.</summary>
    /// <remarks>
    /// <see cref="Token"/> is what ties a WaitFrame on the Game Thread to a submit
    /// on the Render Thread. Every token must be submitted exactly once, in the
    /// order issued; the SDK returns <see cref="NexVRResult.ErrorInvalidFrameToken"/>
    /// rather than letting a violated pairing reach the runtime.
    ///
    /// The views are inline rather than an array because the native struct
    /// embeds them by value up to NEXVR_MAX_VIEWS (4 views for quad-views foveated rendering).
    /// </remarks>
    [StructLayout(LayoutKind.Sequential)]
    public struct FrameState
    {
        public uint StructSize;

        public ulong Token;
        public long PredictedDisplayTime;
        public uint ShouldRender;

        public uint ViewCount;
        public View View0;
        public View View1;
        public View View2;
        public View View3;

        public View GetView(int index)
        {
            switch (index)
            {
                case 0: return View0;
                case 1: return View1;
                case 2: return View2;
                case 3: return View3;
                default: throw new ArgumentOutOfRangeException(nameof(index));
            }
        }
    }

    /// <summary>Complete 6DOF controller tracking and button/analog input state.</summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct ControllerState
    {
        public uint StructSize;
        public uint IsConnected;
        public uint IsTracked;
        public Pose GripPose;
        public Pose AimPose;
        public Vector3f LinearVelocity;
        public Vector3f AngularVelocity;
        public float Trigger;
        public float Grip;
        public Vector2f Thumbstick;
        public uint Buttons;
        public uint Touches;

        public bool Connected => IsConnected != 0;
        public bool Tracked => IsTracked != 0;
        public bool IsButtonPressed(uint buttonMask) => (Buttons & buttonMask) != 0;
        public bool IsButtonTouched(uint touchMask) => (Touches & touchMask) != 0;

        public static ControllerState Create()
        {
            return new ControllerState
            {
                StructSize = (uint)Marshal.SizeOf<ControllerState>()
            };
        }
    }

    public static class Result
    {
        public static bool Succeeded(NexVRResult result) => (int)result >= 0;
        public static bool Failed(NexVRResult result) => (int)result < 0;
    }

    /// <summary>Raw entry points. Prefer a wrapper over calling these directly.</summary>
    public static class Native
    {
        private const string SdkDll = "nexvr_sdk";
        private const string BridgeDll = "nexvr_unity";

        // --- nexvr_sdk.dll --------------------------------------------------

        /// <summary>
        /// Query the runtime BEFORE Unity creates its graphics device.
        /// </summary>
        /// <remarks>
        /// In a Unity plugin this is genuinely difficult — see the adapter section
        /// of docs/B2B_UNITY_INTEGRATION_PLAN.md. Call it as early as possible
        /// (a <c>[RuntimeInitializeOnLoadMethod]</c> with
        /// <c>BeforeSplashScreen</c>) and treat
        /// <see cref="NexVRResult.ErrorWrongAdapter"/> at init as a launch
        /// configuration problem, not a code bug.
        ///
        /// Success does NOT prove a headset is attached: SteamVR returns a valid
        /// system with no HMD present and will accept submitted frames.
        /// </remarks>
        [DllImport(SdkDll, EntryPoint = "NexVR_GetGraphicsRequirements",
                   CallingConvention = CallingConvention.StdCall)]
        public static extern NexVRResult GetGraphicsRequirements(ref GraphicsRequirements requirements);

        [DllImport(SdkDll, EntryPoint = "NexVR_InitializeDX11",
                   CallingConvention = CallingConvention.StdCall)]
        public static extern NexVRResult InitializeDX11(ref InitializeDX11Info info, out IntPtr session);

        /// <summary>Blocks for the runtime's frame interval. GAME THREAD only.</summary>
        [DllImport(SdkDll, EntryPoint = "NexVR_WaitFrame",
                   CallingConvention = CallingConvention.StdCall)]
        public static extern NexVRResult WaitFrame(IntPtr session, ref FrameState frame);

        /// <summary>
        /// RENDER THREAD only. Do not call this from a MonoBehaviour.
        /// </summary>
        /// <remarks>
        /// Declared for completeness and for non-Unity hosts. In Unity this must
        /// reach the Render Thread through the bridge's render event, because the
        /// copy has to be ordered against Unity's own draw calls on the immediate
        /// context — calling it from Update() would issue the copy on the wrong
        /// thread and against an unfinished frame.
        /// </remarks>
        [DllImport(SdkDll, EntryPoint = "NexVR_SubmitFrameDX11",
                   CallingConvention = CallingConvention.StdCall)]
        public static extern NexVRResult SubmitFrameDX11(IntPtr session, ulong token, IntPtr colorTexture);

        /// <summary>
        /// RENDER THREAD only. Submit a frame optionally accompanied by its depth buffer.
        /// </summary>
        [DllImport(SdkDll, EntryPoint = "NexVR_SubmitFrameWithDepthDX11",
                   CallingConvention = CallingConvention.StdCall)]
        public static extern NexVRResult SubmitFrameWithDepthDX11(IntPtr session, ulong token,
                                                                 IntPtr colorTexture, ref DepthInfoDX11 depth);

        /// <summary>
        /// Query whether the runtime supports XR_KHR_composition_layer_depth.
        /// </summary>
        [DllImport(SdkDll, EntryPoint = "NexVR_SupportsDepthSubmission",
                   CallingConvention = CallingConvention.StdCall)]
        public static extern uint SupportsDepthSubmission(IntPtr session);

        [DllImport(SdkDll, EntryPoint = "NexVR_Shutdown",
                   CallingConvention = CallingConvention.StdCall)]
        public static extern void Shutdown(IntPtr session);

        /// <summary>Human-readable detail for the last failure ON THIS THREAD.</summary>
        /// <remarks>
        /// Thread-local in the SDK, so a failure on the Render Thread cannot be
        /// read from the Game Thread. Use the bridge's stats for render-side
        /// outcomes instead.
        ///
        /// Marshalled as <c>IntPtr</c> plus an explicit
        /// <c>PtrToStringAnsi</c> rather than as <c>string</c>: a returned
        /// <c>string</c> makes the default marshaller try to free the buffer with
        /// CoTaskMemFree, and this one belongs to the SDK.
        /// </remarks>
        [DllImport(SdkDll, EntryPoint = "NexVR_GetLastErrorDetail",
                   CallingConvention = CallingConvention.StdCall)]
        private static extern IntPtr GetLastErrorDetailRaw();

        public static string GetLastErrorDetail()
        {
            IntPtr text = GetLastErrorDetailRaw();
            return text == IntPtr.Zero ? string.Empty : Marshal.PtrToStringAnsi(text) ?? string.Empty;
        }

        [DllImport(SdkDll, EntryPoint = "NexVR_InitializeDX12",
                   CallingConvention = CallingConvention.StdCall)]
        public static extern NexVRResult InitializeDX12(ref InitializeDX12Info info, out IntPtr session);

        [DllImport(SdkDll, EntryPoint = "NexVR_SubmitFrameDX12",
                   CallingConvention = CallingConvention.StdCall)]
        public static extern NexVRResult SubmitFrameDX12(IntPtr session, ulong token, IntPtr colorResource, uint colorState);

        [DllImport(SdkDll, EntryPoint = "NexVR_SubmitFrameWithDepthDX12",
                   CallingConvention = CallingConvention.StdCall)]
        public static extern NexVRResult SubmitFrameWithDepthDX12(IntPtr session, ulong token,
                                                                 IntPtr colorResource, uint colorState,
                                                                 ref DepthInfoDX12 depth);

        // --- 6DOF Motion Controller Input & Haptics (SDK Direct) ------------
        [DllImport(SdkDll, EntryPoint = "NexVR_SyncInput",
                   CallingConvention = CallingConvention.StdCall)]
        public static extern NexVRResult SyncInput(IntPtr session);

        [DllImport(SdkDll, EntryPoint = "NexVR_GetControllerState",
                   CallingConvention = CallingConvention.StdCall)]
        public static extern NexVRResult GetControllerState(IntPtr session, NexVRHand hand, ref ControllerState outState);

        [DllImport(SdkDll, EntryPoint = "NexVR_TriggerHaptic",
                   CallingConvention = CallingConvention.StdCall)]
        public static extern NexVRResult TriggerHaptic(IntPtr session, NexVRHand hand, ref HapticFeedback haptic);

        [DllImport(SdkDll, EntryPoint = "NexVR_StopHaptic",
                   CallingConvention = CallingConvention.StdCall)]
        public static extern NexVRResult StopHaptic(IntPtr session, NexVRHand hand);

        /// <summary>Pass to <c>CommandBuffer.IssuePluginEvent</c>.</summary>
        [DllImport(BridgeDll, EntryPoint = "NexVR_Unity_GetRenderEventFunc",
                   CallingConvention = CallingConvention.StdCall)]
        public static extern IntPtr GetRenderEventFunc();

        [DllImport(BridgeDll, EntryPoint = "NexVR_Unity_Attach",
                   CallingConvention = CallingConvention.StdCall)]
        public static extern void Attach(IntPtr session);

        /// <summary>
        /// Stop the Render Thread using the session. Call BEFORE
        /// <see cref="Shutdown"/>, or a queued render event submits against a
        /// destroyed session.
        /// </summary>
        [DllImport(BridgeDll, EntryPoint = "NexVR_Unity_Detach",
                   CallingConvention = CallingConvention.StdCall)]
        public static extern void Detach();

        /// <summary>
        /// Stage a frame with an optional depth buffer from the Game Thread.
        /// Returns the event id to issue, or -1 when every slot is still in flight.
        /// </summary>
        [DllImport(BridgeDll, EntryPoint = "NexVR_Unity_StageFrameWithDepth",
                   CallingConvention = CallingConvention.StdCall)]
        public static extern int StageFrameWithDepth(ulong token, IntPtr nativeTexture,
                                                     IntPtr nativeDepth, float nearZ, float farZ,
                                                     int depthRange);

        /// <summary>
        /// Stage a frame from the Game Thread. Returns the event id to issue, or
        /// -1 when every slot is still in flight.
        /// </summary>
        /// <remarks>
        /// A -1 means the Render Thread is behind. Skip the frame. Do NOT spin
        /// waiting for a slot: blocking the Game Thread on the Render Thread turns
        /// a frame-rate dip into a hang.
        /// </remarks>
        [DllImport(BridgeDll, EntryPoint = "NexVR_Unity_StageFrame",
                   CallingConvention = CallingConvention.StdCall)]
        public static extern int StageFrame(ulong token, IntPtr nativeTexture);

        /// <summary>
        /// Stage a D3D12 frame with an optional depth buffer from the Game Thread.
        /// </summary>
        [DllImport(BridgeDll, EntryPoint = "NexVR_Unity_StageFrameWithDepthDX12",
                   CallingConvention = CallingConvention.StdCall)]
        public static extern int StageFrameWithDepthDX12(ulong token, IntPtr colorResource, uint colorState,
                                                         IntPtr depthResource, uint depthState,
                                                         float nearZ, float farZ, int depthRange);

        /// <summary>
        /// Stage a D3D12 frame from the Game Thread with no depth buffer.
        /// </summary>
        [DllImport(BridgeDll, EntryPoint = "NexVR_Unity_StageFrameDX12",
                   CallingConvention = CallingConvention.StdCall)]
        public static extern int StageFrameDX12(ulong token, IntPtr colorResource, uint colorState);

        /// <summary>
        /// Query whether the current attached session supports depth submission.
        /// </summary>
        [DllImport(BridgeDll, EntryPoint = "NexVR_Unity_SupportsDepthSubmission",
                   CallingConvention = CallingConvention.StdCall)]
        public static extern int UnitySupportsDepthSubmission();

        /// <summary>
        /// Recover Unity's ID3D11Device and immediate context from any texture
        /// Unity created.
        /// </summary>
        /// <remarks>
        /// This is how the integration obtains the device without Unity's native
        /// plugin headers. Both handles come back with a reference that must be
        /// returned through <see cref="ReleaseDeviceHandles"/>.
        /// </remarks>
        [DllImport(BridgeDll, EntryPoint = "NexVR_Unity_GetDeviceFromTexture",
                   CallingConvention = CallingConvention.StdCall)]
        public static extern int GetDeviceFromTexture(IntPtr nativeTexture, out IntPtr device,
                                                      out IntPtr context);

        [DllImport(BridgeDll, EntryPoint = "NexVR_Unity_ReleaseDeviceHandles",
                   CallingConvention = CallingConvention.StdCall)]
        public static extern void ReleaseDeviceHandles(IntPtr device, IntPtr context);

        /// <summary>
        /// Recover Unity's ID3D12Device from an ID3D12Resource Unity created.
        /// </summary>
        [DllImport(BridgeDll, EntryPoint = "NexVR_Unity_GetDeviceFromResourceDX12",
                   CallingConvention = CallingConvention.StdCall)]
        public static extern int GetDeviceFromResourceDX12(IntPtr resource, out IntPtr device);

        [DllImport(BridgeDll, EntryPoint = "NexVR_Unity_ReleaseDeviceHandleDX12",
                   CallingConvention = CallingConvention.StdCall)]
        public static extern void ReleaseDeviceHandleDX12(IntPtr device);

        /// <summary>Render-thread outcomes, which cannot be read any other way.</summary>
        [DllImport(BridgeDll, EntryPoint = "NexVR_Unity_GetStats",
                   CallingConvention = CallingConvention.StdCall)]
        public static extern void GetStats(out uint submitted, out uint dropped, out uint failed,
                                           out int lastResult);

        // --- 6DOF Motion Controller Input & Haptics (Unity Bridge) ----------
        [DllImport(BridgeDll, EntryPoint = "NexVR_Unity_SyncInput",
                   CallingConvention = CallingConvention.StdCall)]
        public static extern int UnitySyncInput();

        [DllImport(BridgeDll, EntryPoint = "NexVR_Unity_GetControllerState",
                   CallingConvention = CallingConvention.StdCall)]
        public static extern int UnityGetControllerState(NexVRHand hand, ref ControllerState outState);

        [DllImport(BridgeDll, EntryPoint = "NexVR_Unity_TriggerHaptic",
                   CallingConvention = CallingConvention.StdCall)]
        public static extern int UnityTriggerHaptic(NexVRHand hand, ref HapticFeedback haptic);

        [DllImport(BridgeDll, EntryPoint = "NexVR_Unity_StopHaptic",
                   CallingConvention = CallingConvention.StdCall)]
        public static extern int UnityStopHaptic(NexVRHand hand);
    }
}
