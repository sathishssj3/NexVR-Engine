// StereixCamera.cs — Official Stereix VR Camera component for Unity
// Copyright (c) 2026 Mesmeran Lab. All rights reserved.
//
// Automatically drives OpenXR stereo views and submits render textures
// with optional hardware depth buffer to the Stereix native engine.

using System;
using UnityEngine;
using UnityEngine.Rendering;

namespace Stereix
{
    [RequireComponent(typeof(Camera))]
    [AddComponentMenu("Stereix/Stereix VR Camera")]
    public sealed class StereixCamera : MonoBehaviour
    {
        [Header("Projection & Clipping")]
        [Tooltip("Near clip plane in meters. The stereo projection is rebuilt from headset FOV.")]
        [SerializeField] private float nearClip = 0.05f;

        [Tooltip("Far clip plane in meters.")]
        [SerializeField] private float farClip = 1000f;

        [Header("Hardware Depth Submission")]
        [Tooltip("Submit depth buffer to OpenXR for sub-millimeter positional timewarp (XR_KHR_composition_layer_depth).")]
        [SerializeField] private bool submitDepth = true;

        [Header("Diagnostics")]
        [SerializeField] private bool logStats = false;

        private Camera _camera;
        private RenderTexture _vrTarget;
        private CommandBuffer _submitBuffer;
        private IntPtr _session = IntPtr.Zero;
        private StereixGraphicsRequirements _reqs;
        private StereixFrameState _frameState;
        private bool _initialized;

        private void Awake()
        {
            _camera = GetComponent<Camera>();
            _frameState = StereixFrameState.Create();
        }

        private void Start()
        {
            InitializeVR();
        }

        private void InitializeVR()
        {
            _reqs = StereixGraphicsRequirements.Create();
            var res = StereixNative.NexVR_GetGraphicsRequirements(ref _reqs);
            if (res.Failed())
            {
                Debug.LogError($"[Stereix] Failed to query OpenXR requirements: {res}. Detail: {StereixNative.GetLastErrorDetail()}");
                return;
            }

            // Create target render texture with exact OpenXR dimensions
            int width = (int)_reqs.requiredTextureWidth;
            int height = (int)_reqs.requiredTextureHeight;
            _vrTarget = new RenderTexture(width, height, 24, RenderTextureFormat.ARGB32)
            {
                name = "Stereix_VR_Target",
                enableRandomWrite = false,
                useMipMap = false,
            };
            _vrTarget.Create();

            IntPtr devPtr = StereixNative.NexVR_Unity_GetDeviceFromTexture(_vrTarget.GetNativeTexturePtr());
            if (devPtr == IntPtr.Zero)
            {
                Debug.LogError("[Stereix] Could not retrieve native D3D11 device from render texture.");
                return;
            }

            var initInfo = StereixInitializeDX11Info.Create();
            initInfo.device = devPtr;
            initInfo.representativeTarget = _vrTarget.GetNativeTexturePtr();
            initInfo.colorSpace = StereixColorSpace.SrgbEncoded;

            res = StereixNative.NexVR_InitializeDX11(ref initInfo, out _session);
            if (res.Failed())
            {
                Debug.LogError($"[Stereix] Failed to initialize Stereix session: {res}. Detail: {StereixNative.GetLastErrorDetail()}");
                return;
            }

            StereixNative.NexVR_Unity_Attach(_session);
            _submitBuffer = new CommandBuffer { name = "Stereix_SubmitFrame" };
            _camera.AddCommandBuffer(CameraEvent.AfterEverything, _submitBuffer);
            _camera.targetTexture = _vrTarget;
            _initialized = true;

            Debug.Log($"[Stereix] VR Session established successfully. Target resolution: {width}x{height}");
        }

        private void Update()
        {
            if (!_initialized || _session == IntPtr.Zero) return;

            var res = StereixNative.NexVR_WaitFrame(_session, ref _frameState);
            if (res.Failed())
            {
                if (res == StereixResult.SessionExiting)
                {
                    Debug.Log("[Stereix] Headset requested session termination.");
                    Shutdown();
                }
                return;
            }

            if (_frameState.shouldRender != 0)
            {
                // Queue render thread submission
                IntPtr colorPtr = _vrTarget.GetNativeTexturePtr();
                IntPtr depthPtr = submitDepth ? _vrTarget.GetNativeDepthBufferPtr() : IntPtr.Zero;

                int slot = StereixNative.NexVR_Unity_StageFrameWithDepth(
                    _frameState.token,
                    colorPtr,
                    depthPtr,
                    nearClip,
                    farClip,
                    StereixDepthRange.Reversed
                );

                if (slot >= 0)
                {
                    _submitBuffer.Clear();
                    _submitBuffer.IssuePluginEvent(StereixNative.NexVR_Unity_GetRenderEventFunc(), slot);
                }
            }
        }

        private void OnDestroy()
        {
            Shutdown();
        }

        private void Shutdown()
        {
            if (!_initialized) return;
            _initialized = false;

            if (_submitBuffer != null && _camera != null)
            {
                _camera.RemoveCommandBuffer(CameraEvent.AfterEverything, _submitBuffer);
                _submitBuffer.Release();
                _submitBuffer = null;
            }

            StereixNative.NexVR_Unity_Detach();

            if (_session != IntPtr.Zero)
            {
                StereixNative.NexVR_Shutdown(_session);
                _session = IntPtr.Zero;
            }

            if (_vrTarget != null)
            {
                _vrTarget.Release();
                Destroy(_vrTarget);
                _vrTarget = null;
            }

            Debug.Log("[Stereix] VR Session terminated cleanly.");
        }
    }
}
