#include <gtest/gtest.h>
#include "src/core/camera_classifier.h"

using namespace vrinject;

TEST(VulkanCameraClassifierTest, ReflectionRejection) {
    Matrix4x4 reflection = { .m = {{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}} };
    reflection.m[1][1] = -1.0f; // Typical reflection scaling

    CameraCandidate candidate;
    // We expect TryCreateCandidate to either reject it or give it a very low score
    bool created = CameraClassifier::TryCreateCandidate(1, nullptr, reflection, candidate);
    if (created) {
        EXPECT_LT(candidate.confidence, 0.5f);
    } else {
        EXPECT_FALSE(created);
    }
}

TEST(VulkanCameraClassifierTest, OrthographicRejection) {
    Matrix4x4 ortho = { .m = {{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}} };
    // Orthographic projection typically has m[3][3] = 1, while perspective has m[3][3] = 0 or close to 0
    ortho.m[3][3] = 1.0f;

    CameraCandidate candidate;
    bool created = CameraClassifier::TryCreateCandidate(1, nullptr, ortho, candidate);
    if (created) {
        EXPECT_LT(candidate.confidence, 0.5f);
    } else {
        EXPECT_FALSE(created);
    }
}
