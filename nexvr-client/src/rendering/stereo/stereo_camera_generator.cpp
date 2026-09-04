#include "rendering/stereo/stereo_camera_generator.h"
#include <cmath>

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

void StereoCameraGenerator::Generate(const RenderFrameSnapshot& renderSnapshot,
                                     const CameraSnapshot& camera, 
                                     const StereoConstants& constants,
                                     EyeView& outLeftEye,
                                     EyeView& outRightEye) {
    bool isLeftHanded = camera.isLeftHanded;
    
    auto createPoseView = [isLeftHanded](const XrPosef& pose) -> Matrix4x4 {
        float qx = pose.orientation.x;
        float qy = pose.orientation.y;
        float qz = pose.orientation.z;
        float qw = pose.orientation.w;
        float tx = pose.position.x;
        float ty = pose.position.y;
        float tz = pose.position.z;

        if (isLeftHanded) {
            tz = -tz;
            qx = -qx;
            qy = -qy;
        }

        float xx = qx * qx, yy = qy * qy, zz = qz * qz;
        float xy = qx * qy, xz = qx * qz, yz = qy * qz;
        float wx = qw * qx, wy = qw * qy, wz = qw * qz;

        Matrix4x4 r;
        r.m[0][0] = 1.0f - 2.0f * (yy + zz);
        r.m[0][1] = 2.0f * (xy + wz);
        r.m[0][2] = 2.0f * (xz - wy);
        r.m[0][3] = 0.0f;

        r.m[1][0] = 2.0f * (xy - wz);
        r.m[1][1] = 1.0f - 2.0f * (xx + zz);
        r.m[1][2] = 2.0f * (yz + wx);
        r.m[1][3] = 0.0f;

        r.m[2][0] = 2.0f * (xz + wy);
        r.m[2][1] = 2.0f * (yz - wx);
        r.m[2][2] = 1.0f - 2.0f * (xx + yy);
        r.m[2][3] = 0.0f;

        r.m[3][0] = 0.0f;
        r.m[3][1] = 0.0f;
        r.m[3][2] = 0.0f;
        r.m[3][3] = 1.0f;

        Matrix4x4 view;
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                view.m[i][j] = r.m[j][i];
            }
            view.m[i][3] = 0.0f;
        }

        view.m[3][0] = -(view.m[0][0] * tx + view.m[1][0] * ty + view.m[2][0] * tz);
        view.m[3][1] = -(view.m[0][1] * tx + view.m[1][1] * ty + view.m[2][1] * tz);
        view.m[3][2] = -(view.m[0][2] * tx + view.m[1][2] * ty + view.m[2][2] * tz);
        view.m[3][3] = 1.0f;

        return view;
    };

    // OpenXR defines FOV relative to the center, so Left angle is negative, Right angle is positive.
    auto createProj = [isLeftHanded](const XrFovf& fov, bool reversedZ, float zn, float zf) -> Matrix4x4 {
        float tanLeft = tanf(fov.angleLeft);
        float tanRight = tanf(fov.angleRight);
        float tanDown = tanf(fov.angleDown);
        float tanUp = tanf(fov.angleUp);

        float tanWidth = tanRight - tanLeft;
        float tanHeight = (tanUp - tanDown);
        
        Matrix4x4 proj = {};
        proj.m[0][0] = 2.0f / tanWidth;
        proj.m[1][1] = 2.0f / tanHeight;
        proj.m[2][0] = (tanRight + tanLeft) / tanWidth;
        proj.m[2][1] = (tanUp + tanDown) / tanHeight;
        proj.m[2][3] = isLeftHanded ? 1.0f : -1.0f;
        
        // Z mapping (DX style)
        if (reversedZ) {
            proj.m[2][2] = isLeftHanded ? -zn / (zf - zn) : zn / (zf - zn) - 1.0f;
            proj.m[3][2] = isLeftHanded ? (zf * zn) / (zf - zn) : (zf * zn) / (zf - zn); 
        } else {
            proj.m[2][2] = isLeftHanded ? zf / (zf - zn) : zf / (zn - zf);
            proj.m[3][2] = isLeftHanded ? -(zf * zn) / (zf - zn) : (zf * zn) / (zn - zf);
        }
        return proj;
    };

    Matrix4x4 leftPoseView = createPoseView(renderSnapshot.leftPose);
    Matrix4x4 rightPoseView = createPoseView(renderSnapshot.rightPose);

    // Eye View = GameView * OpenXRPoseView
    outLeftEye.view = Multiply(camera.view, leftPoseView);
    outRightEye.view = Multiply(camera.view, rightPoseView);

    // Apply the original projection matrix scale factors to the OpenXR FOV.
    // Actually, to use OpenXR's FOV directly, we just build a projection matrix from it.
    // The shader might expect the same near/far planes as the game, which we don't reliably have.
    // We will use the fallback constants for near/far if we don't have them, or approximate them.
    // For now, we use constants.nearPlane and constants.farPlane.
    float nearP = constants.nearPlane > 0.001f ? constants.nearPlane : 0.1f;
    float farP = constants.farPlane > nearP ? constants.farPlane : 1000.0f;

    outLeftEye.projection = createProj(renderSnapshot.leftFov, camera.reversedZ, nearP, farP);
    outRightEye.projection = createProj(renderSnapshot.rightFov, camera.reversedZ, nearP, farP);

    outLeftEye.viewProjection = Multiply(outLeftEye.view, outLeftEye.projection);
    outRightEye.viewProjection = Multiply(outRightEye.view, outRightEye.projection);

    // Optional eye positions (extracted from the updated view matrices)
    auto extractPos = [](const Matrix4x4& v) -> Vector3 {
        // Inverse translation = - v.m[3][0]*R_0 - v.m[3][1]*R_1 - v.m[3][2]*R_2
        Vector3 pos;
        pos.x = -(v.m[3][0] * v.m[0][0] + v.m[3][1] * v.m[0][1] + v.m[3][2] * v.m[0][2]);
        pos.y = -(v.m[3][0] * v.m[1][0] + v.m[3][1] * v.m[1][1] + v.m[3][2] * v.m[1][2]);
        pos.z = -(v.m[3][0] * v.m[2][0] + v.m[3][1] * v.m[2][1] + v.m[3][2] * v.m[2][2]);
        return pos;
    };

    outLeftEye.eyePosition = extractPos(outLeftEye.view);
    outRightEye.eyePosition = extractPos(outRightEye.view);
}

} // namespace vrinject
