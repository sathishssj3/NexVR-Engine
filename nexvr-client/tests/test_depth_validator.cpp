#include <gtest/gtest.h>
#include "heuristics/depth_validator.h"

using namespace vrinject;

TEST(DepthValidator, RejectsFalsePositives) {
    DepthCandidate shadowCascade;
    shadowCascade.identity.width = 1024;
    shadowCascade.identity.height = 1024;
    shadowCascade.identity.arraySize = 4;
    DepthValidator::Validate(shadowCascade, 1920, 1080);
    EXPECT_FALSE(shadowCascade.valid);

    DepthCandidate cubemap;
    cubemap.identity.width = 512;
    cubemap.identity.height = 512;
    cubemap.identity.arraySize = 6;
    DepthValidator::Validate(cubemap, 1920, 1080);
    EXPECT_FALSE(cubemap.valid);

    DepthCandidate mainDepth;
    mainDepth.identity.width = 1920;
    mainDepth.identity.height = 1080;
    mainDepth.identity.arraySize = 1;
    mainDepth.clearCount = 1;
    mainDepth.bindCount = 10;
    DepthValidator::Validate(mainDepth, 1920, 1080);
    EXPECT_TRUE(mainDepth.valid);
    EXPECT_EQ(mainDepth.resolutionScore, 1.0f);
}
