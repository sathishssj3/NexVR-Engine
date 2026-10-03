// Stereix.cs — Official Stereix B2B SDK for Unity
// Copyright (c) 2026 Mesmeran Lab. All rights reserved.
//
// P/Invoke declarations and high-level wrappers for the Stereix B2B SDK and Unity bridge.
// Direct3D 11 & Direct3D 12 native OpenXR rendering with hardware depth submission.

using System;
using System.Runtime.InteropServices;

namespace Stereix
{
    /// <summary>Outcome of every SDK call. Mirrors Stereix_Result in stereix_sdk.h.</summary>
    public enum StereixResult
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
        ErrorInvalidCommandQueue = -14,

        ErrorColorSpaceUnspecified = -20,
        ErrorMultithreadUnavailable = -21,

        ErrorInvalidFrameToken = -30,
        ErrorSwapchainTimeout = -31,

        ErrorInvalidArgument = -40,
        ErrorInvalidSession = -41,
        ErrorRuntimeFailure = -42,
    }

    public static class StereixResultExtensions
    {
        public static bool Succeeded(this StereixResult res) => (int)res >= 0;
        public static bool Failed(this StereixResult res) => (int)res < 0;
    }

    public enum StereixColorSpace
    {
        Unspecified = 0,
        SrgbEncoded = 1,
        Linear = 2,
    }

    public enum StereixDepthRange
    {
        Standard = 0,  // Near = 0.0, Far = 1.0
        Reversed = 1,  // Near = 1.0, Far = 0.0 (Unity standard)
    }

    public enum StereixHand
    {
        Left = 0,
        Right = 1,
    }

    public enum StereixTrackingOrigin
    {
        Local = 0, // Seated / Head-relative
        Stage = 1, // Room-scale / Floor-relative
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct StereixGraphicsRequirements
    {
        public uint structSize;
        public long adapterLuid;
        public uint minFeatureLevel;
        public uint recommendedWidth;
        public uint recommendedHeight;
        public uint maxWidth;
        public uint maxHeight;
        public uint viewCount;
        public uint requiredTextureWidth;
        public uint requiredTextureHeight;

        public static StereixGraphicsRequirements Create()
        {
            return new StereixGraphicsRequirements
            {
                structSize = (uint)Marshal.SizeOf<StereixGraphicsRequirements>()
            };
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct StereixInitializeDX11Info
    {
        public uint structSize;
        public IntPtr device;
        public IntPtr immediateContext;
        public IntPtr representativeTarget;
        public StereixColorSpace colorSpace;

        public static StereixInitializeDX11Info Create()
        {
            return new StereixInitializeDX11Info
            {
                structSize = (uint)Marshal.SizeOf<StereixInitializeDX11Info>()
            };
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct StereixDepthInfoDX11
    {
        public uint structSize;
        public IntPtr depthTexture;
        public float nearZ;
        public float farZ;
        public StereixDepthRange range;

        public static StereixDepthInfoDX11 Create()
        {
            return new StereixDepthInfoDX11
            {
                structSize = (uint)Marshal.SizeOf<StereixDepthInfoDX11>()
            };
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct StereixFov
    {
        public float angleLeft;
        public float angleRight;
        public float angleUp;
        public float angleDown;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct StereixVector3
    {
        public float x, y, z;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct StereixQuaternion
    {
        public float x, y, z, w;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct StereixPose
    {
        public StereixQuaternion orientation;
        public StereixVector3 position;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct StereixView
    {
        public StereixPose pose;
        public StereixFov fov;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct StereixFrameState
    {
        public uint structSize;
        public ulong token;
        public long predictedDisplayTime;
        public uint shouldRender;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 2)]
        public StereixView[] views;

        public static StereixFrameState Create()
        {
            return new StereixFrameState
            {
                structSize = (uint)Marshal.SizeOf<StereixFrameState>(),
                views = new StereixView[2]
            };
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct StereixTrackingState
    {
        public uint structSize;
        public StereixPose headPose;
        public StereixVector3 linearVelocity;
        public StereixVector3 angularVelocity;
        public uint isPositionTracked;
        public uint isOrientationTracked;

        public static StereixTrackingState Create()
        {
            return new StereixTrackingState
            {
                structSize = (uint)Marshal.SizeOf<StereixTrackingState>()
            };
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct StereixControllerState
    {
        public uint structSize;
        public uint isConnected;
        public uint isTracked;
        public StereixPose gripPose;
        public StereixPose aimPose;
        public StereixVector3 linearVelocity;
        public StereixVector3 angularVelocity;
        public float trigger;
        public float grip;
        public float thumbstickX;
        public float thumbstickY;
        public uint buttons;

        public static StereixControllerState Create()
        {
            return new StereixControllerState
            {
                structSize = (uint)Marshal.SizeOf<StereixControllerState>()
            };
        }
    }

    /// <summary>
    /// Direct P/Invoke bindings into the native Stereix / NexVR engine DLLs.
    /// </summary>
    /// <summary>
    /// Direct P/Invoke bindings into the native Stereix Engine DLLs (stereix_sdk.dll & stereix_unity.dll).
    /// </summary>
    public static class StereixNative
    {
        private const string SdkDll = "stereix_sdk.dll";
        private const string UnityBridgeDll = "stereix_unity.dll";

        // --- Core Stereix SDK Functions ---
        [DllImport(SdkDll, CallingConvention = CallingConvention.StdCall)]
        public static extern StereixResult Stereix_GetGraphicsRequirements(ref StereixGraphicsRequirements requirements);

        [DllImport(SdkDll, CallingConvention = CallingConvention.StdCall)]
        public static extern StereixResult Stereix_InitializeDX11(ref StereixInitializeDX11Info info, out IntPtr outSession);

        [DllImport(SdkDll, CallingConvention = CallingConvention.StdCall)]
        public static extern StereixResult Stereix_WaitFrame(IntPtr session, ref StereixFrameState frameState);

        [DllImport(SdkDll, CallingConvention = CallingConvention.StdCall)]
        public static extern StereixResult Stereix_SubmitFrameDX11(IntPtr session, ulong frameToken, IntPtr renderTarget);

        [DllImport(SdkDll, CallingConvention = CallingConvention.StdCall)]
        public static extern StereixResult Stereix_SubmitFrameWithDepthDX11(IntPtr session, ulong frameToken, IntPtr renderTarget, ref StereixDepthInfoDX11 depthInfo);

        [DllImport(SdkDll, CallingConvention = CallingConvention.StdCall)]
        public static extern StereixResult Stereix_SyncInput(IntPtr session);

        [DllImport(SdkDll, CallingConvention = CallingConvention.StdCall)]
        public static extern StereixResult Stereix_GetControllerState(IntPtr session, StereixHand hand, ref StereixControllerState outState);

        [DllImport(SdkDll, CallingConvention = CallingConvention.StdCall)]
        public static extern StereixResult Stereix_TriggerHaptic(IntPtr session, StereixHand hand, ref StereixHapticFeedback haptic);

        [DllImport(SdkDll, CallingConvention = CallingConvention.StdCall)]
        public static extern StereixResult Stereix_StopHaptic(IntPtr session, StereixHand hand);

        [DllImport(SdkDll, CallingConvention = CallingConvention.StdCall)]
        public static extern void Stereix_Shutdown(IntPtr session);

        [DllImport(SdkDll, CallingConvention = CallingConvention.StdCall)]
        private static extern IntPtr Stereix_GetLastErrorDetail();

        public static string GetLastErrorDetail()
        {
            IntPtr ptr = Stereix_GetLastErrorDetail();
            return ptr == IntPtr.Zero ? string.Empty : Marshal.PtrToStringAnsi(ptr);
        }

        // --- Stereix Unity Native Render Bridge Functions ---
        [DllImport(UnityBridgeDll, CallingConvention = CallingConvention.StdCall)]
        public static extern IntPtr Stereix_Unity_GetRenderEventFunc();

        [DllImport(UnityBridgeDll, CallingConvention = CallingConvention.StdCall)]
        public static extern int Stereix_Unity_StageFrameWithDepth(ulong token, IntPtr colorTex, IntPtr depthTex, float nearZ, float farZ, StereixDepthRange range);

        [DllImport(UnityBridgeDll, CallingConvention = CallingConvention.StdCall)]
        public static extern void Stereix_Unity_Attach(IntPtr session);

        [DllImport(UnityBridgeDll, CallingConvention = CallingConvention.StdCall)]
        public static extern void Stereix_Unity_Detach();

        [DllImport(UnityBridgeDll, CallingConvention = CallingConvention.StdCall)]
        public static extern IntPtr Stereix_Unity_GetDeviceFromTexture(IntPtr textureNativePtr);

        // --- Backwards-Compatibility Aliases ---
        public static StereixResult NexVR_GetGraphicsRequirements(ref StereixGraphicsRequirements reqs) => Stereix_GetGraphicsRequirements(ref reqs);
        public static StereixResult NexVR_InitializeDX11(ref StereixInitializeDX11Info info, out IntPtr session) => Stereix_InitializeDX11(ref info, out session);
        public static StereixResult NexVR_WaitFrame(IntPtr session, ref StereixFrameState frame) => Stereix_WaitFrame(session, ref frame);
        public static StereixResult NexVR_SubmitFrameDX11(IntPtr session, ulong token, IntPtr rt) => Stereix_SubmitFrameDX11(session, token, rt);
        public static StereixResult NexVR_SubmitFrameWithDepthDX11(IntPtr session, ulong token, IntPtr rt, ref StereixDepthInfoDX11 depth) => Stereix_SubmitFrameWithDepthDX11(session, token, rt, ref depth);
        public static StereixResult NexVR_GetControllerState(IntPtr session, StereixHand hand, ref StereixControllerState state) => Stereix_GetControllerState(session, hand, ref state);
        public static void NexVR_Shutdown(IntPtr session) => Stereix_Shutdown(session);
        public static IntPtr NexVR_Unity_GetRenderEventFunc() => Stereix_Unity_GetRenderEventFunc();
        public static int NexVR_Unity_StageFrameWithDepth(ulong token, IntPtr color, IntPtr depth, float nearZ, float farZ, StereixDepthRange range) => Stereix_Unity_StageFrameWithDepth(token, color, depth, nearZ, farZ, range);
        public static void NexVR_Unity_Attach(IntPtr session) => Stereix_Unity_Attach(session);
        public static void NexVR_Unity_Detach() => Stereix_Unity_Detach();
        public static IntPtr NexVR_Unity_GetDeviceFromTexture(IntPtr tex) => Stereix_Unity_GetDeviceFromTexture(tex);
    }
}
