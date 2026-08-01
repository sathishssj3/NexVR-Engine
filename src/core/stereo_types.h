#pragma once

#include "math_types.h"
#include "camera_snapshot.h"
#include "depth_snapshot.h"
#include <cstdint>

namespace vrinject {

struct EyeView {
    Matrix4x4 view;
    Matrix4x4 projection;
    Matrix4x4 viewProjection;
    Vector3 eyePosition;
};

struct StereoConstants {
    float ipd = 0.063f;          // 63mm IPD by default
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
    float convergence = 5.0f;    // Distance where parallax is zero
};

struct StereoViewport {
    uint32_t width = 0;
    uint32_t height = 0;
    float topLeftX = 0.0f;
    float topLeftY = 0.0f;
    float minDepth = 0.0f;
    float maxDepth = 1.0f;
};

struct StereoFrameContext {
    uint64_t frameId = 0;
    
    CameraSnapshot camera;
    DepthSnapshot depth;

    EyeView leftEye;
    EyeView rightEye;
    
    // Shader constants inputs
    Matrix4x4 inverseViewProjection;
    
    StereoConstants constants;
    StereoViewport viewport;
    
    bool IsValid() const {
        return camera.IsValid() && depth.IsValid();
    }
};

} // namespace vrinject
