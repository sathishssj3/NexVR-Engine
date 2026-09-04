#include <gtest/gtest.h>
#include "heuristics/camera_classifier.h"

using namespace vrinject;

TEST(CameraClassifierTest, RejectsInvalidMatrix) {
    Matrix4x4 m = {}; // All zeros
    bool rowMajor, revZ;
    EXPECT_FALSE(CameraClassifier::ClassifyMatrix(m, rowMajor, revZ));
}

TEST(CameraClassifierTest, DetectsPerspective) {
    Matrix4x4 m = {};
    // Typical perspective matrix
    m.m[0][0] = 1.0f;
    m.m[1][1] = 1.0f;
    m.m[2][2] = 1.0f; // z scale
    m.m[2][3] = 1.0f; // w = z
    m.m[3][3] = 0.0f; // w from z
    
    bool rowMajor, revZ;
    EXPECT_TRUE(CameraClassifier::ClassifyMatrix(m, rowMajor, revZ));
    EXPECT_TRUE(rowMajor);
}

TEST(CameraClassifierTest, CreatesCandidate) {
    Matrix4x4 m = {};
    m.m[0][0] = 1.0f;
    m.m[1][1] = 1.0f;
    m.m[2][2] = 1.0f;
    m.m[2][3] = 1.0f;
    m.m[3][3] = 0.0f;
    
    CameraCandidate candidate;
    EXPECT_TRUE(CameraClassifier::TryCreateCandidate(123, nullptr, m, candidate));
    EXPECT_EQ(candidate.id, 123);
    EXPECT_TRUE(candidate.valid);
    EXPECT_TRUE(candidate.rowMajor);
}
