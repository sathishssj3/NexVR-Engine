#include <gtest/gtest.h>
#include "../src/core/depth_classifier.h"
#include <dxgiformat.h>

using namespace vrinject;

TEST(DepthClassifier, ClassifiesFormatsCorrectly) {
    DepthCandidate candHigh;
    candHigh.identity.format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
    DepthClassifier::Classify(candHigh);
    EXPECT_FLOAT_EQ(candHigh.classifierScore, 1.0f);
    
    DepthCandidate candStandard;
    candStandard.identity.format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    DepthClassifier::Classify(candStandard);
    EXPECT_FLOAT_EQ(candStandard.classifierScore, 0.8f);
    
    DepthCandidate candLow;
    candLow.identity.format = DXGI_FORMAT_D16_UNORM;
    DepthClassifier::Classify(candLow);
    EXPECT_FLOAT_EQ(candLow.classifierScore, 0.5f);
    
    DepthCandidate candInvalid;
    candInvalid.identity.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    DepthClassifier::Classify(candInvalid);
    EXPECT_FALSE(candInvalid.valid);
    EXPECT_FLOAT_EQ(candInvalid.classifierScore, 0.0f);
}
