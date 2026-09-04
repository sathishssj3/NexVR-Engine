#include <gtest/gtest.h>
#include "rendering/stereo/stereo_camera_generator.h"
#include "rendering/stereo/stereo_frame_builder.h"

using namespace vrinject;

TEST(StereoCameraGenerator, TranslationCorrectness) {
    CameraSnapshot cam;
    cam.view = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };
    cam.projection = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 1,
        0, 0, -0.1f, 0
    };
    cam.position = { 0, 0, 0 };

    StereoConstants constants;
    constants.ipd = 0.063f;
    constants.convergence = 1.0f;

    EyeView left, right;
    RenderFrameSnapshot dummyFrame;
    dummyFrame.leftPose.orientation.w = 1.0f;
    dummyFrame.rightPose.orientation.w = 1.0f;
    // OpenXR pose gives world space translation from head.
    dummyFrame.leftPose.position.x = -0.0315f;
    dummyFrame.rightPose.position.x = 0.0315f;
    StereoCameraGenerator::Generate(dummyFrame, cam, constants, left, right);

    // Left eye translates right in view space, so m[3][0] increases
    EXPECT_FLOAT_EQ(left.view.m[3][0], 0.0315f);
    EXPECT_FLOAT_EQ(right.view.m[3][0], -0.0315f);

    // Left eye translates left in world space
    EXPECT_FLOAT_EQ(left.eyePosition.x, -0.0315f);
    EXPECT_FLOAT_EQ(right.eyePosition.x, 0.0315f);
}

TEST(StereoCameraGenerator, ProjectionShift) {
    CameraSnapshot cam;
    cam.view = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };
    cam.projection = {
        2.0f, 0, 0, 0, // xScale = 2.0
        0, 2.0f, 0, 0,
        0, 0, 1, 1,
        0, 0, -0.1f, 0
    };
    cam.position = { 0, 0, 0 };

    StereoConstants constants;
    constants.ipd = 0.063f;
    constants.convergence = 1.0f;

    EyeView left, right;
    RenderFrameSnapshot dummyFrame;
    dummyFrame.leftPose.orientation.w = 1.0f;
    dummyFrame.rightPose.orientation.w = 1.0f;
    
    // We want (tanRight + tanLeft) / (tanRight - tanLeft) = 0.063f
    // If tanRight = 1.063 and tanLeft = -0.937, sum = 0.126, diff = 2.0. 0.126 / 2.0 = 0.063
    dummyFrame.leftFov.angleRight = atan(1.063f);
    dummyFrame.leftFov.angleLeft = atan(-0.937f);
    dummyFrame.leftFov.angleUp = atan(1.0f);
    dummyFrame.leftFov.angleDown = atan(-1.0f);

    dummyFrame.rightFov.angleRight = atan(0.937f);
    dummyFrame.rightFov.angleLeft = atan(-1.063f);
    dummyFrame.rightFov.angleUp = atan(1.0f);
    dummyFrame.rightFov.angleDown = atan(-1.0f);

    StereoCameraGenerator::Generate(dummyFrame, cam, constants, left, right);

    // Shift magnitude = (halfIpd * xScale) / convergence
    // = (0.0315 * 2.0) / 1.0 = 0.063
    EXPECT_FLOAT_EQ(left.projection.m[2][0], 0.063f);
    EXPECT_FLOAT_EQ(right.projection.m[2][0], -0.063f);
}

TEST(StereoFrameBuilder, InverseMath) {
    CameraSnapshot cam;
    cam.view = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };
    // Identity for simplicity
    cam.vp = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };

    DepthSnapshot depth;
    EyeView left, right;
    StereoConstants constants;

    StereoFrameContext ctx = StereoFrameBuilder::Build(1, cam, depth, left, right, constants, 1920, 1080);
    
    // Inverse of identity is identity
    EXPECT_FLOAT_EQ(ctx.inverseViewProjection.m[0][0], 1.0f);
    EXPECT_FLOAT_EQ(ctx.inverseViewProjection.m[3][3], 1.0f);
}
