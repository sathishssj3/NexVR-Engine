// Drives a NexVR session from a Unity camera.
//
// ============================================================================
// SCOPE — READ BEFORE ATTACHING THIS TO ANYTHING
// ============================================================================
//
// BUILT-IN RENDER PIPELINE ONLY. It calls Camera.Render() explicitly, twice per
// frame, which URP and HRP do not support — they own the render loop and expose
// their own injection points. A URP/HRP version needs a different hook, not a
// tweak to this file.
//
// NOTHING IN THIS FILE HAS BEEN RUN. The native side it drives is measured and
// tested; this script is written against Unity's documented API and has never
// executed. Treat every number in it as a starting point, especially the
// stereo convergence and the near/far planes.
//
// ============================================================================
// THE ONE THING THAT MAKES THIS NON-TRIVIAL
// ============================================================================
//
// NexVR_SubmitFrameDX11 must run on the thread that owns the D3D11 immediate
// context. In Unity that is the Render Thread, and this MonoBehaviour runs on
// the Game Thread. The only sanctioned crossing is
// CommandBuffer.IssuePluginEvent, which carries a single int — so the frame's
// token and texture are staged in a native ring buffer and the int is the slot
// index. See nexvr_unity_bridge.cpp.

using System;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.Experimental.Rendering;
using UnityEngine.Rendering;

namespace NexVR
{
    [RequireComponent(typeof(Camera))]
    public sealed class NexVRCamera : MonoBehaviour
    {
        [Header("Projection")]
        [Tooltip("Near clip for the VR projection. Unity's camera near plane is ignored — " +
                 "the projection is rebuilt from the runtime's FOV angles every frame.")]
        [SerializeField] private float nearClip = 0.05f;

        [SerializeField] private float farClip = 1000f;

        [Header("Depth")]
        [Tooltip("Submit depth buffer to OpenXR for improved reprojection and occlusion (XR_KHR_composition_layer_depth).")]
        [SerializeField] private bool submitDepth = true;

        [Header("Diagnostics")]
        [Tooltip("Reads a pixel back after every copy and fails if it did not land. " +
                 "Costs a full CPU/GPU sync per frame. Bring-up only — never ship with this on.")]
        [SerializeField] private bool verifyCopies = false;

        [SerializeField] private bool logStatsEverySecond = false;

        private Camera _camera;
        private RenderTexture _vrTarget;
        private CommandBuffer _submitBuffer;
        private IntPtr _session = IntPtr.Zero;
        private IntPtr _renderEventFunc = IntPtr.Zero;

        private IntPtr _device = IntPtr.Zero;
        private IntPtr _context = IntPtr.Zero;

        private GraphicsRequirements _requirements;
        private bool _running;
        private bool _supportsDepth;

        /// <summary>The transform the head pose is applied relative to.</summary>
        /// <remarks>
        /// The camera's own transform is overwritten every frame from the runtime,
        /// so anything that wants to move the player — a character controller, a
        /// teleport system — must move this instead. Without it, gameplay movement
        /// and head tracking fight each other and the head wins.
        /// </remarks>
        private Transform _rig;

        private float _statsTimer;

        // ====================================================================
        // Lifecycle
        // ====================================================================

        private void Awake()
        {
            _camera = GetComponent<Camera>();

            // A parent to carry gameplay movement. Created rather than required,
            // because a studio attaching this to an existing camera should not
            // have to restructure their hierarchy to find out whether it works.
            _rig = transform.parent;
            if (_rig == null)
            {
                var rig = new GameObject("NexVR Rig").transform;
                rig.SetPositionAndRotation(transform.position, transform.rotation);
                transform.SetParent(rig, worldPositionStays: true);
                _rig = rig;
            }
        }

        private bool _isD3D12 = false;

        private void OnEnable()
        {
            if (SystemInfo.graphicsDeviceType == GraphicsDeviceType.Direct3D12)
            {
                _isD3D12 = true;
            }
            else if (SystemInfo.graphicsDeviceType == GraphicsDeviceType.Direct3D11)
            {
                _isD3D12 = false;
            }
            else
            {
                Debug.LogError(
                    $"[NexVR] This project is running on {SystemInfo.graphicsDeviceType}. " +
                    "The NexVR SDK supports Direct3D 11 and Direct3D 12. Set Project Settings > Player > " +
                    "Graphics APIs to Direct3D11 or Direct3D12.");
                enabled = false;
                return;
            }

            if (!Initialize())
            {
                enabled = false;
            }
        }

        private void OnDisable() => Teardown();

        private bool Initialize()
        {
            // --- 1. What does the runtime require? --------------------------
            _requirements = new GraphicsRequirements { StructSize = (uint)Marshal.SizeOf<GraphicsRequirements>() };

            NexVRResult result = Native.GetGraphicsRequirements(ref _requirements);
            if (Result.Failed(result))
            {
                Debug.LogError($"[NexVR] {result}: {Native.GetLastErrorDetail()}");
                return false;
            }

            Debug.Log($"[NexVR] runtime wants {_requirements.RequiredTextureWidth}x" +
                      $"{_requirements.RequiredTextureHeight} " +
                      $"({_requirements.ViewCount} views at {_requirements.RecommendedWidth}x" +
                      $"{_requirements.RecommendedHeight}), adapter LUID " +
                      $"{_requirements.AdapterLuidHigh:X8}:{_requirements.AdapterLuidLow:X8}");

            // --- 2. The render target, at exactly the size demanded ---------
            // R8G8B8A8_UNorm, not RenderTextureFormat.ARGB32. Measured on Unity
            // 6000.5.9f1: both land on DXGI R8G8B8A8_TYPELESS, which is the right
            // family — but BGRA32 lands on B8G8R8A8_TYPELESS and is refused
            // outright. Naming the GraphicsFormat removes Unity's discretion.
            var descriptor = new RenderTextureDescriptor(
                (int)_requirements.RequiredTextureWidth,
                (int)_requirements.RequiredTextureHeight)
            {
                graphicsFormat = GraphicsFormat.R8G8B8A8_UNorm,
                depthBufferBits = 24,

                // MSAA off. A multisampled resource cannot be CopyResource'd at
                // all, so this is not a quality preference — it is the copy path
                // refusing to exist. Resolve to this target if you want AA.
                msaaSamples = 1,
                useMipMap = false,
                sRGB = true,
            };

            _vrTarget = new RenderTexture(descriptor) { name = "NexVR Submit Target" };
            if (!_vrTarget.Create())
            {
                Debug.LogError("[NexVR] could not create the VR render target.");
                return false;
            }

            IntPtr nativeTexture = _vrTarget.GetNativeTexturePtr();
            if (nativeTexture == IntPtr.Zero)
            {
                Debug.LogError("[NexVR] the render target has no native pointer.");
                return false;
            }

            // --- 3. Unity's device, via the texture that knows it -----------
            if (_isD3D12)
            {
                if (Native.GetDeviceFromResourceDX12(nativeTexture, out _device) == 0)
                {
                    Debug.LogError("[NexVR] could not recover the D3D12 device from the render target.");
                    return false;
                }

                var info = new InitializeDX12Info
                {
                    StructSize = (uint)Marshal.SizeOf<InitializeDX12Info>(),
                    Device = _device,
                    CommandQueue = IntPtr.Zero,
                    RepresentativeTarget = nativeTexture,
                    ColorSpace = NexVRColorSpace.SrgbEncoded,
                    EnableCopyVerification = verifyCopies ? 1u : 0u,
                    ViewConfiguration = (uint)NexVRViewConfiguration.Stereo,
                };

                result = Native.InitializeDX12(ref info, out _session);
            }
            else
            {
                if (Native.GetDeviceFromTexture(nativeTexture, out _device, out _context) == 0)
                {
                    Debug.LogError("[NexVR] could not recover the D3D11 device from the render target.");
                    return false;
                }

                var info = new InitializeDX11Info
                {
                    StructSize = (uint)Marshal.SizeOf<InitializeDX11Info>(),
                    Device = _device,
                    ImmediateContext = _context,
                    RepresentativeTarget = nativeTexture,
                    ColorSpace = NexVRColorSpace.SrgbEncoded,
                    EnableCopyVerification = verifyCopies ? 1u : 0u,
                    ViewConfiguration = (uint)NexVRViewConfiguration.Stereo,
                };

                result = Native.InitializeDX11(ref info, out _session);
            }

            if (Result.Failed(result))
            {
                Debug.LogError($"[NexVR] {result}: {Native.GetLastErrorDetail()}");

                if (result == NexVRResult.ErrorWrongAdapter)
                {
                    Debug.LogError(
                        "[NexVR] Unity created its graphics device on a different GPU than the " +
                        "OpenXR runtime requires. Unity's device exists before any script runs, " +
                        "so this cannot be corrected from C#. Relaunch with -force-device-index N, " +
                        "or pin the executable to the high-performance GPU in Windows Graphics " +
                        "settings.");
                }
                return false;
            }

            string note = Native.GetLastErrorDetail();
            if (!string.IsNullOrEmpty(note))
            {
                Debug.LogWarning($"[NexVR] {note}");
            }

            // --- 5. The render-thread bridge --------------------------------
            _renderEventFunc = Native.GetRenderEventFunc();
            if (_renderEventFunc == IntPtr.Zero)
            {
                Debug.LogError("[NexVR] the bridge returned no render-event function.");
                return false;
            }

            Native.Attach(_session);
            _supportsDepth = Native.SupportsDepthSubmission(_session) != 0;
            if (_supportsDepth)
            {
                Debug.Log("[NexVR] OpenXR runtime supports XR_KHR_composition_layer_depth. Depth submission enabled.");
            }
            else
            {
                Debug.Log("[NexVR] OpenXR runtime does not support depth submission. Falling back to color-only.");
            }

            _submitBuffer = new CommandBuffer { name = "NexVR Submit" };
            _camera.targetTexture = _vrTarget;

            _running = true;
            Debug.Log("[NexVR] session running.");
            return true;
        }

        private void Teardown()
        {
            _running = false;

            if (_session != IntPtr.Zero)
            {
                Native.Detach();
                Native.Shutdown(_session);
                _session = IntPtr.Zero;
            }

            if (_isD3D12)
            {
                if (_device != IntPtr.Zero)
                {
                    Native.ReleaseDeviceHandleDX12(_device);
                    _device = IntPtr.Zero;
                }
            }
            else
            {
                if (_device != IntPtr.Zero || _context != IntPtr.Zero)
                {
                    Native.ReleaseDeviceHandles(_device, _context);
                    _device = IntPtr.Zero;
                    _context = IntPtr.Zero;
                }
            }

            if (_camera != null)
            {
                _camera.targetTexture = null;
            }

            _submitBuffer?.Release();
            _submitBuffer = null;

            if (_vrTarget != null)
            {
                _vrTarget.Release();
                Destroy(_vrTarget);
                _vrTarget = null;
            }
        }

        // ====================================================================
        // The frame
        // ====================================================================

        /// <summary>
        /// LateUpdate, not Update.
        /// </summary>
        /// <remarks>
        /// NexVR_WaitFrame blocks for roughly one display interval and returns the
        /// pose predicted for when this frame will actually be shown. Applying it
        /// in Update means every script that moves the player afterwards is acting
        /// on a stale head position, and the result reads as head-tracking lag
        /// that no amount of tuning the runtime will fix.
        /// </remarks>
        private void LateUpdate()
        {
            if (!_running)
            {
                return;
            }

            // Synchronize 6DOF controller input and actions with OpenXR runtime
            Bridge.UnitySyncInput();

            var frame = new FrameState { StructSize = (uint)Marshal.SizeOf<FrameState>() };
            NexVRResult waited = Native.WaitFrame(_session, ref frame);

            if (waited == NexVRResult.SessionExiting)
            {
                Debug.Log("[NexVR] the runtime asked the session to exit.");
                Teardown();
                enabled = false;
                return;
            }

            if (Result.Failed(waited))
            {
                Debug.LogError($"[NexVR] WaitFrame {waited}: {Native.GetLastErrorDetail()}");
                Teardown();
                enabled = false;
                return;
            }

            // Token 0 means the session is not in a frame-submitting state yet —
            // normal during startup. Keep ticking; do not tear down.
            if (frame.Token == 0)
            {
                return;
            }

            // shouldRender == 0 still requires the token to be submitted, so the
            // runtime's frame pairing stays intact. The bridge and SDK handle a
            // frame with nothing drawn into it.
            if (frame.ShouldRender != 0)
            {
                RenderBothEyes(frame);
            }

            Submit(frame.Token);
            ReportStats();
        }

        /// <summary>Renders each eye into its slice of the target texture.</summary>
        private void RenderBothEyes(FrameState frame)
        {
            uint views = Math.Min(frame.ViewCount, 4u);
            if (views == 0)
            {
                views = 2u;
            }

            for (uint eye = 0; eye < views; ++eye)
            {
                View view = frame.GetView((int)eye);

                ApplyPose(view.Pose);

                _camera.projectionMatrix = ProjectionFrom(view, nearClip, farClip);

                // Normalised viewport: divides the texture evenly across views (2 for stereo, 4 for quad-views).
                _camera.rect = new Rect(eye / (float)views, 0f, 1f / views, 1f);

                _camera.Render();
            }

            // Left as the right eye's pose deliberately. Anything reading
            // Camera.main.transform after this — audio listeners, UI raycasts —
            // gets a real head pose rather than whatever the last gameplay frame
            // left behind.
        }

        /// <summary>
        /// Applies an OpenXR pose to the camera, in the rig's local space.
        /// </summary>
        /// <remarks>
        /// OpenXR is right-handed with -Z forward. Unity is left-handed with +Z
        /// forward. The conversion is to flip Z on the position and flip X and Y
        /// on the quaternion:
        ///
        ///     position (x, y, z)      ->  (x, y, -z)
        ///     rotation (x, y, z, w)   ->  (-x, -y, z, w)
        ///
        /// Getting this wrong is not subtle — the world moves the wrong way when
        /// the head turns, which is instantly nauseating. It is worth checking
        /// first if anything about the tracking feels inverted.
        /// </remarks>
        private void ApplyPose(Pose pose)
        {
            var localPosition = new Vector3(pose.PositionX, pose.PositionY, -pose.PositionZ);
            var localRotation = new Quaternion(-pose.OrientationX, -pose.OrientationY,
                                               pose.OrientationZ, pose.OrientationW);

            transform.localPosition = localPosition;
            transform.localRotation = localRotation;
        }

        /// <summary>
        /// Builds an off-centre projection from the runtime's four FOV angles.
        /// </summary>
        /// <remarks>
        /// A VR frustum is NOT symmetric — the lens centre is off-axis, and each
        /// eye's four half-angles differ. Camera.fieldOfView cannot express that,
        /// so the matrix is built directly.
        ///
        /// OpenGL-style clip depth (z in [-1, 1]) because that is what Unity's
        /// Camera.projectionMatrix expects; Unity converts to the platform's
        /// convention itself via GL.GetGPUProjectionMatrix.
        /// </remarks>
        private static Matrix4x4 ProjectionFrom(View view, float near, float far)
        {
            float tanLeft = Mathf.Tan(view.FovLeft);
            float tanRight = Mathf.Tan(view.FovRight);
            float tanUp = Mathf.Tan(view.FovUp);
            float tanDown = Mathf.Tan(view.FovDown);

            float width = tanRight - tanLeft;
            float height = tanUp - tanDown;

            var m = new Matrix4x4();
            m[0, 0] = 2f / width;
            m[0, 2] = (tanRight + tanLeft) / width;
            m[1, 1] = 2f / height;
            m[1, 2] = (tanUp + tanDown) / height;
            m[2, 2] = -(far + near) / (far - near);
            m[2, 3] = -(2f * far * near) / (far - near);
            m[3, 2] = -1f;
            return m;
        }

        /// <summary>Hands this frame to the Render Thread.</summary>
        private void Submit(ulong token)
        {
            IntPtr nativeTexture = _vrTarget.GetNativeTexturePtr();
            int slot;

            int depthRange = SystemInfo.usesReversedZBuffer ? (int)NexVRDepthRange.Reversed : (int)NexVRDepthRange.ZeroToOne;

            if (_isD3D12)
            {
                if (submitDepth && _supportsDepth)
                {
                    IntPtr nativeDepth = _vrTarget.GetNativeDepthBufferPtr();
                    slot = Native.StageFrameWithDepthDX12(token, nativeTexture, 0, nativeDepth, 0, nearClip, farClip, depthRange);
                }
                else
                {
                    slot = Native.StageFrameDX12(token, nativeTexture, 0);
                }
            }
            else
            {
                if (submitDepth && _supportsDepth)
                {
                    IntPtr nativeDepth = _vrTarget.GetNativeDepthBufferPtr();
                    slot = Native.StageFrameWithDepth(token, nativeTexture, nativeDepth, nearClip, farClip, depthRange);
                }
                else
                {
                    slot = Native.StageFrame(token, nativeTexture);
                }
            }

            if (slot < 0)
            {
                // Every ring slot is still in flight — the Render Thread is
                // behind. Skip. Do NOT spin waiting: blocking the Game Thread on
                // the Render Thread turns a frame-rate dip into a hang.
                //
                // The token is dropped with it. The SDK tolerates that; what it
                // will not tolerate is the same token submitted twice.
                return;
            }

            _submitBuffer.Clear();
            _submitBuffer.IssuePluginEvent(_renderEventFunc, slot);

            // ExecuteCommandBuffer rather than Camera.AddCommandBuffer: the eyes
            // were rendered by explicit Camera.Render() calls above, and this
            // queues the submit behind them in the same command stream. A camera
            // event would fire per-Render(), i.e. once per eye, and submit the
            // frame before the second eye existed.
            Graphics.ExecuteCommandBuffer(_submitBuffer);
        }

        private void ReportStats()
        {
            if (!logStatsEverySecond)
            {
                return;
            }

            _statsTimer += Time.unscaledDeltaTime;
            if (_statsTimer < 1f)
            {
                return;
            }
            _statsTimer = 0f;

            // The only way to see render-thread outcomes. NexVR_GetLastErrorDetail
            // is thread-local, so a failure over there is invisible from here.
            Native.GetStats(out uint submitted, out uint dropped, out uint failed,
                            out int lastResult);

            if (failed > 0)
            {
                Debug.LogWarning($"[NexVR] submitted {submitted}, dropped {dropped}, " +
                                 $"FAILED {failed}, last result {(NexVRResult)lastResult}");
            }
            else
            {
                Debug.Log($"[NexVR] submitted {submitted}, dropped {dropped} " +
                          "(dropped frames are survivable — the compositor was busy)");
            }
        }

        // ====================================================================
        // 6DOF Controller Input & Haptics API
        // ====================================================================

        /// <summary>Synchronize OpenXR controller input manually from game logic.</summary>
        public static bool SyncInput()
        {
            return Bridge.UnitySyncInput() == (int)NexVRResult.Success;
        }

        /// <summary>Query full 6DOF tracking and button/analog state for the specified hand.</summary>
        public static bool GetControllerState(NexVRHand hand, out ControllerState state)
        {
            state = ControllerState.Create();
            return Bridge.UnityGetControllerState(hand, ref state) == (int)NexVRResult.Success;
        }

        /// <summary>Trigger vibration haptic feedback on a controller hand.</summary>
        public static bool TriggerHaptic(NexVRHand hand, float durationMs, float amplitude, float frequencyHz = 0.0f)
        {
            var haptic = HapticFeedback.Create(durationMs, amplitude, frequencyHz);
            return Bridge.UnityTriggerHaptic(hand, ref haptic) == (int)NexVRResult.Success;
        }

        /// <summary>Immediately stop any active vibration on a controller hand.</summary>
        public static bool StopHaptic(NexVRHand hand)
        {
            return Bridge.UnityStopHaptic(hand) == (int)NexVRResult.Success;
        }
    }
}
