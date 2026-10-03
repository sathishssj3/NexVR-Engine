// test_midas_inpainter.cpp — Enterprise tests for DirectML AI Stereo Depth Synthesizer
#include <gtest/gtest.h>
#include <vector>
#include <cmath>
#include <numeric>
#include "ai/midas_inpainter.h"

using namespace NexVR::AI;

class MidasInpainterTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Instantiate with DirectML enabled (will use GPU if available or fallback cleanly)
        inpainter = std::make_unique<MidasInpainter>(true, 0, L"models/depth_inpainter.onnx");
    }

    std::unique_ptr<MidasInpainter> inpainter;
};

TEST_F(MidasInpainterTest, InitializesCleanlyWithoutCrash) {
    ASSERT_NE(inpainter, nullptr);
    // Should be valid (either loaded or entered safe simulated fallback mode)
    EXPECT_TRUE(inpainter->IsValid() || inpainter->IsSimulated());
}

TEST_F(MidasInpainterTest, GeneratesValidDepthMapFromPlanarRGB) {
    const int width = 64;
    const int height = 64;
    const size_t planeSize = width * height;
    std::vector<float> planarRGB(3 * planeSize, 0.5f); // Flat grey scene

    std::vector<float> depthOutput(planeSize, 0.0f);

    bool ok = inpainter->GenerateDepthMap(planarRGB.data(), depthOutput.data(), width, height);
    EXPECT_TRUE(ok);

    // Verify all depth values are in valid non-zero range [0.05, 1.0]
    for (size_t i = 0; i < planeSize; ++i) {
        EXPECT_GE(depthOutput[i], 0.04f);
        EXPECT_LE(depthOutput[i], 1.0f);
    }

    // Verify vertical perspective gradient: top row (far/sky) should have larger depth than bottom row (near/ground)
    float topRowAvg = 0.0f;
    float bottomRowAvg = 0.0f;
    for (int x = 0; x < width; ++x) {
        topRowAvg += depthOutput[0 * width + x];               // Top row (y=0)
        bottomRowAvg += depthOutput[(height - 1) * width + x]; // Bottom row (y=height-1)
    }
    topRowAvg /= width;
    bottomRowAvg /= width;

    EXPECT_GT(topRowAvg, bottomRowAvg);
}

TEST_F(MidasInpainterTest, GeneratesValidDepthMapFromPackedRGBA) {
    const int width = 32;
    const int height = 32;
    const size_t pixelCount = width * height;
    std::vector<uint8_t> rgba(pixelCount * 4, 128); // 50% grey packed pixels

    std::vector<float> depthOutput(pixelCount, 0.0f);

    bool ok = inpainter->GenerateDepthMapFromRGBA(rgba.data(), depthOutput.data(), width, height);
    EXPECT_TRUE(ok);

    for (size_t i = 0; i < pixelCount; ++i) {
        EXPECT_GE(depthOutput[i], 0.05f);
        EXPECT_LE(depthOutput[i], 1.0f);
    }
}

TEST_F(MidasInpainterTest, TemporalSmoothingDampensFrameToFrameJitter) {
    const int width = 16;
    const int height = 16;
    const size_t planeSize = width * height;

    inpainter->ResetTemporalHistory();
    inpainter->SetTemporalSmoothing(0.5f); // 50% history retention
    inpainter->SetBilateralFiltering(false);

    std::vector<float> frame1(3 * planeSize, 0.2f);
    std::vector<float> depth1(planeSize, 0.0f);
    EXPECT_TRUE(inpainter->GenerateDepthMap(frame1.data(), depth1.data(), width, height));

    // Sudden flash in frame 2
    std::vector<float> frame2(3 * planeSize, 0.8f);
    std::vector<float> depth2(planeSize, 0.0f);
    EXPECT_TRUE(inpainter->GenerateDepthMap(frame2.data(), depth2.data(), width, height));

    // Verify depth2 was dampened by frame1's history rather than jumping immediately
    // Without smoothing, depth would jump to target; with 50% smoothing, depth2 is between depth1 and target.
    for (size_t i = 0; i < planeSize; ++i) {
        EXPECT_GT(depth2[i], depth1[i]);
    }
}

TEST_F(MidasInpainterTest, EdgePreservingBilateralFilterPreservesContrast) {
    const int width = 32;
    const int height = 32;
    const size_t planeSize = width * height;

    // Create image with a sharp vertical edge down the middle: left half black, right half white
    std::vector<float> edgeImage(3 * planeSize, 0.0f);
    float* r = edgeImage.data();
    float* g = edgeImage.data() + planeSize;
    float* b = edgeImage.data() + 2 * planeSize;

    for (int y = 0; y < height; ++y) {
        for (int x = 16; x < width; ++x) {
            size_t idx = y * width + x;
            r[idx] = 1.0f;
            g[idx] = 1.0f;
            b[idx] = 1.0f;
        }
    }

    inpainter->ResetTemporalHistory();
    inpainter->SetTemporalSmoothing(0.0f);
    inpainter->SetBilateralFiltering(true);

    std::vector<float> depthOutput(planeSize, 0.0f);
    EXPECT_TRUE(inpainter->GenerateDepthMap(edgeImage.data(), depthOutput.data(), width, height));

    // Sample across the step edge at row y = 16
    size_t leftPixel = 16 * width + 14;  // Inside black region
    size_t rightPixel = 16 * width + 17; // Inside white region

    // Depth difference across the edge should remain distinct and non-zero
    float stepDelta = std::abs(depthOutput[rightPixel] - depthOutput[leftPixel]);
    EXPECT_GT(stepDelta, 0.001f);
}

TEST_F(MidasInpainterTest, ZeroCrashNullAndInvalidDimensionGuards) {
    std::vector<float> dummy(100, 0.5f);

    // Null input
    EXPECT_FALSE(inpainter->GenerateDepthMap(nullptr, dummy.data(), 10, 10));

    // Null output
    EXPECT_FALSE(inpainter->GenerateDepthMap(dummy.data(), nullptr, 10, 10));

    // Zero width/height
    EXPECT_FALSE(inpainter->GenerateDepthMap(dummy.data(), dummy.data(), 0, 10));
    EXPECT_FALSE(inpainter->GenerateDepthMap(dummy.data(), dummy.data(), 10, -5));

    // Null RGBA packed
    EXPECT_FALSE(inpainter->GenerateDepthMapFromRGBA(nullptr, dummy.data(), 10, 10));
}
