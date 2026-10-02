#include <gtest/gtest.h>
#include "memory_scanner/camera_delta_tracker.h"
#include "memory_scanner/page_scanner.h"
#include "memory_scanner/pointer_chain_resolver.h"
#include "hooks/input_hook.h"
#include <cmath>
#include <vector>

using namespace vrinject;

TEST(CameraDeltaTrackerTest, ViewMatrixValidationFloat) {
    // Identity view matrix (looking forward from origin)
    float identityView[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 5.0f, 1.0f
    };
    EXPECT_TRUE(PageScanner::IsValidViewMatrixFloat(identityView));

    // Rotated 90 degrees around Y axis
    float rotatedView[16] = {
        0.0f, 0.0f, -1.0f, 0.0f,
        0.0f, 1.0f,  0.0f, 0.0f,
        1.0f, 0.0f,  0.0f, 0.0f,
        10.0f, -2.0f, 5.0f, 1.0f
    };
    EXPECT_TRUE(PageScanner::IsValidViewMatrixFloat(rotatedView));

    // Invalid: non-normalized basis vector
    float invalidScaled[16] = {
        2.5f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    EXPECT_FALSE(PageScanner::IsValidViewMatrixFloat(invalidScaled));

    // Invalid: non-orthogonal vectors
    float nonOrthogonal[16] = {
        0.707f, 0.707f, 0.0f, 0.0f,
        0.707f, 0.707f, 0.0f, 0.0f,
        0.0f,   0.0f,   1.0f, 0.0f,
        0.0f,   0.0f,   0.0f, 1.0f
    };
    EXPECT_FALSE(PageScanner::IsValidViewMatrixFloat(nonOrthogonal));
}

TEST(CameraDeltaTrackerTest, ViewMatrixValidationDouble) {
    double identityViewD[16] = {
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, -10.0, 1.0
    };
    EXPECT_TRUE(PageScanner::IsValidViewMatrixDouble(identityViewD));

    double invalidD[16] = { 0.0 };
    EXPECT_FALSE(PageScanner::IsValidViewMatrixDouble(invalidD));
}

TEST(CameraDeltaTrackerTest, ProjectionMatrixValidation) {
    // Row-major standard DX perspective projection
    float projRowMajor[16] = {
        1.3f, 0.0f, 0.0f,  0.0f,
        0.0f, 1.7f, 0.0f,  0.0f,
        0.0f, 0.0f, 1.001f, 1.0f,
        0.0f, 0.0f, -0.1f, 0.0f
    };
    EXPECT_TRUE(PageScanner::IsValidProjectionMatrixFloat(projRowMajor, 90.0f));

    // Column-major perspective projection
    float projColMajor[16] = {
        1.3f, 0.0f, 0.0f,   0.0f,
        0.0f, 1.7f, 0.0f,   0.0f,
        0.0f, 0.0f, 1.001f, -0.1f,
        0.0f, 0.0f, 1.0f,   0.0f
    };
    EXPECT_TRUE(PageScanner::IsValidProjectionMatrixFloat(projColMajor, 90.0f));

    // Non-perspective matrix (w-divide is zero)
    float orthoMat[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    EXPECT_FALSE(PageScanner::IsValidProjectionMatrixFloat(orthoMat, 90.0f));
}

TEST(CameraDeltaTrackerTest, InputHookDeltaAccumulation) {
    InputHook& input = InputHook::GetInstance();
    
    // Drain any prior residual deltas
    float mDelta = 0.0f, sDelta = 0.0f;
    input.ConsumeAccumulatedInputDeltas(mDelta, sDelta);

    // Record mouse movement
    input.RecordPhysicalMouseDelta(30, 40);
    input.RecordThumbstickDelta(0.5f, 0.5f);

    input.ConsumeAccumulatedInputDeltas(mDelta, sDelta);
    EXPECT_NEAR(mDelta, 50.0f, 0.1f); // sqrt(30^2 + 40^2) = 50
    EXPECT_GT(sDelta, 0.5f);

    // Subsequent call must return 0 (consumed)
    input.ConsumeAccumulatedInputDeltas(mDelta, sDelta);
    EXPECT_EQ(mDelta, 0.0f);
    EXPECT_EQ(sDelta, 0.0f);
}

TEST(CameraDeltaTrackerTest, PointerChainResolutionAndDereference) {
    PointerChainResolver resolver;

    // Simulate module memory buffer: static pointer -> dynamic object -> matrix
    uint8_t dummyModule[1024] = {};
    float targetMatrix[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    // Object base address
    uint8_t* objectBase = reinterpret_cast<uint8_t*>(targetMatrix) - 0x40;

    // Module base points to objectBase at offset 0x80
    uintptr_t* staticPtrSlot = reinterpret_cast<uintptr_t*>(dummyModule + 0x80);
    *staticPtrSlot = reinterpret_cast<uintptr_t>(objectBase);

    resolver.SetModuleBounds(dummyModule, sizeof(dummyModule));

    PointerChain chain;
    bool resolved = resolver.ResolveChain(reinterpret_cast<uint8_t*>(targetMatrix), chain, 0x1000);
    EXPECT_TRUE(resolved);
    EXPECT_EQ(chain.baseOffset, 0x80);
    ASSERT_EQ(chain.offsets.size(), 1u);
    EXPECT_EQ(chain.offsets[0], 0x40);

    // Dereference chain
    uint8_t* derefAddr = resolver.DereferenceChain(chain);
    EXPECT_EQ(derefAddr, reinterpret_cast<uint8_t*>(targetMatrix));
}

TEST(CameraDeltaTrackerTest, PointerChainCacheSerialization) {
    PointerChainResolver resolver;

    PointerChain originalChain;
    originalChain.baseOffset = 0x1A40;
    originalChain.offsets = { 0x30, 0x180 };
    originalChain.isDoublePrecision = true;
    originalChain.valid = true;

    std::string testExe = "TestCameraGame.exe";
    EXPECT_TRUE(resolver.SaveCache(testExe, originalChain));

    PointerChain loadedChain;
    EXPECT_TRUE(resolver.LoadCache(testExe, loadedChain));
    EXPECT_TRUE(loadedChain.valid);
    EXPECT_EQ(loadedChain.baseOffset, originalChain.baseOffset);
    EXPECT_EQ(loadedChain.offsets, originalChain.offsets);
    EXPECT_EQ(loadedChain.isDoublePrecision, originalChain.isDoublePrecision);
}
