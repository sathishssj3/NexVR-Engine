// StereixController.cs — Official Stereix 6DOF VR Controller Component for Unity
// Copyright (c) 2026 Mesmeran Lab. All rights reserved.
//
// Automatically syncs 6DOF hand controller tracking and input from OpenXR.

using System;
using UnityEngine;

namespace Stereix
{
    [DisallowMultipleComponent]
    [AddComponentMenu("Stereix/Stereix VR Controller")]
    public sealed class StereixController : MonoBehaviour
    {
        public enum HandType
        {
            Left = 0,
            Right = 1
        }

        [Header("Controller Identification")]
        [Tooltip("Hand identity: Left or Right controller.")]
        [SerializeField] private HandType hand = HandType.Right;

        [Tooltip("When true, tracking uses the Aim pose (forward pointing ray for guns/pointers). " +
                 "When false, tracking uses the Grip pose (hand palm and physical tool grip).")]
        [SerializeField] private bool useAimPose = false;

        [Tooltip("Automatically hide/disable renderers when controller tracking is lost.")]
        [SerializeField] private bool hideWhenUntracked = true;

        [Header("Input Deadzone")]
        [Range(0f, 0.5f)]
        [SerializeField] private float thumbstickDeadzone = 0.05f;

        private StereixControllerState _state;
        private Renderer[] _renderers;
        private bool _wasTracked;

        public HandType Hand => hand;
        public bool IsConnected => _state.isConnected != 0;
        public bool IsTracked => useAimPose ? (_state.aimPoseValid != 0) : (_state.gripPoseValid != 0);
        public StereixControllerState RawState => _state;

        public float Trigger => _state.trigger;
        public float Grip => _state.grip;
        public Vector2 Thumbstick => ApplyDeadzone(new Vector2(_state.thumbstickX, _state.thumbstickY));

        private void Awake()
        {
            _renderers = GetComponentsInChildren<Renderer>(true);
            _state = StereixControllerState.Create();
        }

        private void Update()
        {
            UpdateTracking();
        }

        private void UpdateTracking()
        {
            // Sync input from OpenXR runtime
            var res = StereixNative.Stereix_GetControllerState(
                IntPtr.Zero, // Active session
                (StereixHand)hand,
                ref _state
            );

            if (res.Failed() || !IsConnected)
            {
                if (_wasTracked && hideWhenUntracked)
                {
                    SetRenderersActive(false);
                }
                _wasTracked = false;
                return;
            }

            var pose = useAimPose ? _state.aimPose : _state.gripPose;

            // Convert OpenXR right-handed (+X right, +Y up, -Z forward)
            // to Unity left-handed (+X right, +Y up, +Z forward)
            transform.localPosition = new Vector3(pose.position[0], pose.position[1], -pose.position[2]);
            transform.localRotation = new Quaternion(-pose.orientation[0], -pose.orientation[1], pose.orientation[2], pose.orientation[3]);

            if (!_wasTracked && hideWhenUntracked)
            {
                SetRenderersActive(true);
            }
            _wasTracked = true;
        }

        public void TriggerHaptic(float amplitude = 0.5f, float durationMs = 50f, float frequencyHz = 0f)
        {
            var haptic = StereixHapticFeedback.Create();
            haptic.amplitude = Mathf.Clamp01(amplitude);
            haptic.durationMs = Mathf.Max(0f, durationMs);
            haptic.frequencyHz = frequencyHz;

            StereixNative.Stereix_TriggerHaptic(IntPtr.Zero, (StereixHand)hand, ref haptic);
        }

        private Vector2 ApplyDeadzone(Vector2 input)
        {
            if (input.magnitude < thumbstickDeadzone)
            {
                return Vector2.zero;
            }
            return input;
        }

        private void SetRenderersActive(bool active)
        {
            if (_renderers == null) return;
            for (int i = 0; i < _renderers.Length; i++)
            {
                if (_renderers[i] != null)
                {
                    _renderers[i].enabled = active;
                }
            }
        }
    }
}
