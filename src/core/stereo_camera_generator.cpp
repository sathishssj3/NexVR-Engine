#include "stereo_camera_generator.h"

namespace vrinject {

// Helper to multiply two 4x4 matrices
static Matrix4x4 Multiply(const Matrix4x4& a, const Matrix4x4& b) {
    Matrix4x4 result;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            result.m[i][j] = a.m[i][0] * b.m[0][j] +
                             a.m[i][1] * b.m[1][j] +
                             a.m[i][2] * b.m[2][j] +
                             a.m[i][3] * b.m[3][j];
        }
    }
    return result;
}

void StereoCameraGenerator::Generate(const CameraSnapshot& camera, 
                                     const StereoConstants& constants,
                                     EyeView& outLeftEye,
                                     EyeView& outRightEye) {
    // The offset is half the IPD. Left eye goes left (-), Right eye goes right (+).
    float halfIpd = constants.ipd * 0.5f;
    
    outLeftEye.view = camera.view;
    outRightEye.view = camera.view;

    outLeftEye.projection = camera.projection;
    outRightEye.projection = camera.projection;

    // 1. View Matrix Translation
    // Assuming standard row-major DX view matrix where m[3][0] is the X translation.
    // Moving the eye left by halfIpd means moving the world right in view space (+halfIpd).
    outLeftEye.view.m[3][0] += halfIpd;
    // Moving the eye right by halfIpd means moving the world left in view space (-halfIpd).
    outRightEye.view.m[3][0] -= halfIpd;

    // 2. Off-Axis Projection Shift
    // To ensure zero disparity at the convergence depth, we skew the projection matrix.
    // The X scale (M11) is at m[0][0].
    float xScale = camera.projection.m[0][0];
    
    // Calculate the horizontal shift for the projection matrix (m[2][0] in row-major).
    float shiftMagnitude = (halfIpd * xScale) / constants.convergence;
    
    // Left eye is shifted to the right on the screen
    outLeftEye.projection.m[2][0] += shiftMagnitude;
    // Right eye is shifted to the left on the screen
    outRightEye.projection.m[2][0] -= shiftMagnitude;

    // 3. ViewProjection Matrices
    outLeftEye.viewProjection = Multiply(outLeftEye.view, outLeftEye.projection);
    outRightEye.viewProjection = Multiply(outRightEye.view, outRightEye.projection);

    // 4. Eye Positions (Optional, useful for shaders or debugging)
    // Extract Right vector from view matrix to shift original position
    Vector3 rightVec = { camera.view.m[0][0], camera.view.m[1][0], camera.view.m[2][0] };
    
    outLeftEye.eyePosition = {
        camera.position.x - rightVec.x * halfIpd,
        camera.position.y - rightVec.y * halfIpd,
        camera.position.z - rightVec.z * halfIpd
    };

    outRightEye.eyePosition = {
        camera.position.x + rightVec.x * halfIpd,
        camera.position.y + rightVec.y * halfIpd,
        camera.position.z + rightVec.z * halfIpd
    };
}

} // namespace vrinject
