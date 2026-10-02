#include <gtest/gtest.h>
#include "heuristics/camera_validator.h"

using namespace vrinject;

TEST(CameraValidatorTest, RejectsInvalidCandidate) {
    CameraCandidate c = {};
    c.valid = false;
    EXPECT_FALSE(CameraValidator::ValidateCandidate(c));
}

TEST(CameraValidatorTest, RejectsLowConfidence) {
    CameraCandidate c = {};
    c.valid = true;
    c.confidence = 0.2f;
    EXPECT_FALSE(CameraValidator::ValidateCandidate(c));
}

TEST(CameraValidatorTest, RejectsStaticLongLived) {
    CameraCandidate c = {};
    c.valid = true;
    c.confidence = 0.8f;
    c.temporalScore = 0.05f; // very static
    c.updateCount = 15;      // long lived
    EXPECT_FALSE(CameraValidator::ValidateCandidate(c));
}

TEST(CameraValidatorTest, AcceptsValidCamera) {
    CameraCandidate c = {};
    c.valid = true;
    c.confidence = 0.8f;
    c.temporalScore = 0.6f;
    c.updateCount = 20;
    EXPECT_TRUE(CameraValidator::ValidateCandidate(c));
}
