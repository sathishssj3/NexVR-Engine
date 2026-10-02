// Drives a 6DOF motion controller from an OpenXR session via NexVR.
//
// ============================================================================
// SCOPE & COORDINATE SYSTEM
// ============================================================================
//
// Attach this MonoBehaviour to a GameObject representing the Left or Right
// motion controller in your player rig hierarchy (typically a child of the
// rig transform, alongside NexVRCamera).
//
// OpenXR uses a right-handed coordinate system (+X Right, +Y Up, -Z Forward).
// Unity uses a left-handed coordinate system (+X Right, +Y Up, +Z Forward).
// This component performs the standard coordinate conversion:
//
//     position (x, y, z)      ->  (x, y, -z)
//     rotation (x, y, z, w)   ->  (-x, -y, z, w)
//
// Poses are applied in local space relative to the parent rig transform.

using System;
using System.Runtime.InteropServices;
using UnityEngine;

namespace NexVR
{
    [DisallowMultipleComponent]
    public sealed class NexVRController : MonoBehaviour
    {
        [Header("Controller Configuration")]
        [Tooltip("Hand identity: Left or Right controller.")]
        [SerializeField] private NexVRHand hand = NexVRHand.Right;

        [Tooltip("When true, tracking uses the Aim pose (forward ray/pointer alignment for guns and UI laser pointers). " +
                 "When false, tracking uses the Grip pose (hand palm and physical tool grip alignment).")]
        [SerializeField] private bool useAimPose = false;

        [Tooltip("If true, this component automatically synchronizes OpenXR input actions each frame. " +
                 "If false, you must call NexVR.Native.UnitySyncInput() elsewhere in your game loop.")]
        [SerializeField] private bool autoSyncInput = true;

        [Tooltip("Automatically hide/disable MeshRenderers and child visuals when controller tracking is lost.")]
        [SerializeField] private bool hideWhenUntracked = true;

        [Header("Input Deadzone")]
        [Range(0f, 0.5f)]
        [SerializeField] private float thumbstickDeadzone = 0.05f;

        private ControllerState _state;
        private Renderer[] _renderers;
        private bool _wasTracked;

        // ====================================================================
        // Public Properties & State Accessors
        // ====================================================================

        public NexVRHand Hand => hand;
        public bool IsConnected => _state.Connected;
        public bool IsTracked => _state.Tracked;
        public ControllerState RawState => _state;

        /// <summary>Analog index trigger axis value in range [0.0, 1.0].</summary>
        public float Trigger => _state.Trigger;

        /// <summary>Analog hand grip / squeeze axis value in range [0.0, 1.0].</summary>
        public float Grip => _state.Grip;

        /// <summary>Thumbstick 2D vector filtered with radial deadzone.</summary>
        public Vector2 Thumbstick
        {
            get
            {
                var raw = new Vector2(_state.Thumbstick.X, _state.Thumbstick.Y);
                float magnitude = raw.magnitude;
                if (magnitude <= thumbstickDeadzone)
                {
                    return Vector2.zero;
                }
                return raw.normalized * ((magnitude - thumbstickDeadzone) / (1f - thumbstickDeadzone));
            }
        }

        /// <summary>Linear velocity converted to Unity space (meters/second).</summary>
        public Vector3 Velocity => new Vector3(_state.LinearVelocity.X, _state.LinearVelocity.Y, -_state.LinearVelocity.Z);

        /// <summary>Angular velocity converted to Unity space (radians/second).</summary>
        public Vector3 AngularVelocity => new Vector3(-_state.AngularVelocity.X, -_state.AngularVelocity.Y, _state.AngularVelocity.Z);

        /// <summary>Check if a digital button bitmask is currently pressed.</summary>
        public bool IsButtonPressed(uint buttonMask) => _state.IsButtonPressed(buttonMask);

        /// <summary>Check if a capacitive touch sensor bitmask is currently active.</summary>
        public bool IsButtonTouched(uint touchMask) => _state.IsButtonTouched(touchMask);

        // Convenience shortcuts for standard buttons
        public bool TriggerButton => _state.IsButtonPressed(NexVRButton.Trigger);
        public bool GripButton => _state.IsButtonPressed(NexVRButton.Grip);
        public bool PrimaryButton => _state.IsButtonPressed(NexVRButton.Primary);     // A on Right, X on Left
        public bool SecondaryButton => _state.IsButtonPressed(NexVRButton.Secondary); // B on Right, Y on Left
        public bool ThumbstickButton => _state.IsButtonPressed(NexVRButton.Thumbstick);
        public bool MenuButton => _state.IsButtonPressed(NexVRButton.Menu);

        // ====================================================================
        // Lifecycle
        // ====================================================================

        private void Awake()
        {
            _state = ControllerState.Create();
            _renderers = GetComponentsInChildren<Renderer>(includeInactive: true);
        }

        private void OnEnable()
        {
            _wasTracked = false;
        }

        private void Update()
        {
            // Sync actions across all input paths once per frame if designated
            if (autoSyncInput)
            {
                Native.UnitySyncInput();
            }

            // Read the latest 6DOF controller tracking and analog state
            int result = Native.UnityGetControllerState(hand, ref _state);
            if (result != 0)
            {
                // Session not ready or detached
                SetVisualsVisible(false);
                return;
            }

            bool tracked = _state.Tracked;
            if (tracked)
            {
                Pose pose = useAimPose ? _state.AimPose : _state.GripPose;
                ApplyPose(pose);
            }

            if (tracked != _wasTracked)
            {
                _wasTracked = tracked;
                if (hideWhenUntracked)
                {
                    SetVisualsVisible(tracked);
                }
            }
        }

        // ====================================================================
        // Haptics
        // ====================================================================

        /// <summary>
        /// Trigger vibration haptics on this controller.
        /// </summary>
        /// <param name="durationSeconds">Duration in seconds (e.g. 0.1f for a click, 0.5f for an explosion).</param>
        /// <param name="amplitude">Vibration amplitude in range [0.0, 1.0].</param>
        /// <param name="frequencyHz">Vibration frequency in Hz (0.0 uses runtime default).</param>
        /// <returns>True if the haptic command was accepted by OpenXR.</returns>
        public bool TriggerHaptic(float durationSeconds, float amplitude, float frequencyHz = 0.0f)
        {
            var haptic = HapticFeedback.FromSeconds(durationSeconds, amplitude, frequencyHz);
            int res = Native.UnityTriggerHaptic(hand, ref haptic);
            return res == 0;
        }

        /// <summary>
        /// Trigger vibration haptics on this controller with duration in milliseconds.
        /// </summary>
        /// <param name="durationMs">Duration in milliseconds (e.g. 50.0f for a light click).</param>
        /// <param name="amplitude">Vibration amplitude in range [0.0, 1.0].</param>
        /// <param name="frequencyHz">Vibration frequency in Hz (0.0 uses runtime default).</param>
        public bool TriggerHapticMs(float durationMs, float amplitude, float frequencyHz = 0.0f)
        {
            var haptic = HapticFeedback.Create(durationMs, amplitude, frequencyHz);
            int res = Native.UnityTriggerHaptic(hand, ref haptic);
            return res == 0;
        }

        /// <summary>
        /// Immediately stop any active vibration on this controller.
        /// </summary>
        public bool StopHaptic()
        {
            int res = Native.UnityStopHaptic(hand);
            return res == 0;
        }

        // ====================================================================
        // Internal Helpers
        // ====================================================================

        private void ApplyPose(Pose pose)
        {
            transform.localPosition = new Vector3(pose.PositionX, pose.PositionY, -pose.PositionZ);
            transform.localRotation = new Quaternion(-pose.OrientationX, -pose.OrientationY,
                                                     pose.OrientationZ, pose.OrientationW);
        }

        private void SetVisualsVisible(bool visible)
        {
            if (_renderers == null) return;
            for (int i = 0; i < _renderers.Length; ++i)
            {
                if (_renderers[i] != null)
                {
                    _renderers[i].enabled = visible;
                }
            }
        }
    }
}
