// test_unreal_bridge.cpp — Functional and concurrency tests for Unreal Engine 5 Bridge

#include <gtest/gtest.h>
#include <d3d11.h>
#include <d3d12.h>
#include <vector>

#include "nexvr_sdk.h"
#include "nexvr_unreal_bridge.h"

class UnrealBridgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Ensure clean slate before each test
        NexVR_Unreal_Detach();
    }

    void TearDown() override {
        NexVR_Unreal_Detach();
    }
};

TEST_F(UnrealBridgeTest, AttachAndDetachLifecycle) {
    auto dummySession = reinterpret_cast<NexVR_Session>(uintptr_t(0x12345678ULL));
    NexVR_Unreal_Attach(dummySession);

    uint32_t submitted = 10, dropped = 10, failed = 10;
    int32_t lastResult = -99;
    NexVR_Unreal_GetStats(&submitted, &dropped, &failed, &lastResult);

    EXPECT_EQ(submitted, 0u);
    EXPECT_EQ(dropped, 0u);
    EXPECT_EQ(failed, 0u);
    EXPECT_EQ(lastResult, NEXVR_SUCCESS);

    NexVR_Unreal_Detach();
    // After detach, depth submission query must return 0
    EXPECT_EQ(NexVR_Unreal_SupportsDepthSubmission(), 0);
}

TEST_F(UnrealBridgeTest, NullColorTextureRejected) {
    int slot = NexVR_Unreal_StageFrame(1001, nullptr);
    EXPECT_EQ(slot, -1);

    slot = NexVR_Unreal_StageFrameDX12(1002, nullptr, 0x4);
    EXPECT_EQ(slot, -1);

    slot = NexVR_Unreal_StageFrameWithDepthDX12(1003, nullptr, 0x4, nullptr, 0, 0.1f, 100.0f, 1);
    EXPECT_EQ(slot, -1);
}

TEST_F(UnrealBridgeTest, StageFrameDX11Success) {
    auto dummyColor = reinterpret_cast<void*>(uintptr_t(0xAAAA0000ULL));
    int slot = NexVR_Unreal_StageFrame(101, dummyColor);
    EXPECT_GE(slot, 0);
    EXPECT_LT(slot, 8);

    // Consume slot so it frees
    NexVR_Unreal_ProcessRenderCommand(slot);
}

TEST_F(UnrealBridgeTest, StageFrameWithDepthDX11Success) {
    auto dummyColor = reinterpret_cast<void*>(uintptr_t(0xAAAA0000ULL));
    auto dummyDepth = reinterpret_cast<void*>(uintptr_t(0xBBBB0000ULL));
    int slot = NexVR_Unreal_StageFrameWithDepth(102, dummyColor, dummyDepth, 0.05f, 1000.0f, 1);
    EXPECT_GE(slot, 0);
    EXPECT_LT(slot, 8);

    NexVR_Unreal_ProcessRenderCommand(slot);
}

TEST_F(UnrealBridgeTest, StageFrameDX12Success) {
    auto dummyColor = reinterpret_cast<void*>(uintptr_t(0xCCCC0000ULL));
    uint32_t colorState = 0x4; // D3D12_RESOURCE_STATE_RENDER_TARGET
    int slot = NexVR_Unreal_StageFrameDX12(201, dummyColor, colorState);
    EXPECT_GE(slot, 0);
    EXPECT_LT(slot, 8);

    NexVR_Unreal_ProcessRenderCommand(slot);
}

TEST_F(UnrealBridgeTest, StageFrameWithDepthDX12Success) {
    auto dummyColor = reinterpret_cast<void*>(uintptr_t(0xCCCC0000ULL));
    auto dummyDepth = reinterpret_cast<void*>(uintptr_t(0xDDDD0000ULL));
    uint32_t colorState = 0x4;  // RENDER_TARGET
    uint32_t depthState = 0x20; // DEPTH_READ
    int slot = NexVR_Unreal_StageFrameWithDepthDX12(202, dummyColor, colorState,
                                                    dummyDepth, depthState,
                                                    1000.0f, 0.1f, 1); // Reverse-Z
    EXPECT_GE(slot, 0);
    EXPECT_LT(slot, 8);

    NexVR_Unreal_ProcessRenderCommand(slot);
}

TEST_F(UnrealBridgeTest, RingBufferExhaustionAndReclaim) {
    auto dummyColor = reinterpret_cast<void*>(uintptr_t(0xEEEE0000ULL));

    // Fill all 8 slots
    std::vector<int> claimedSlots;
    for (int i = 0; i < 8; ++i) {
        int slot = NexVR_Unreal_StageFrame(1000 + i, dummyColor);
        EXPECT_EQ(slot, i);
        claimedSlots.push_back(slot);
    }

    // 9th frame must be refused with -1 (render thread is behind)
    int overflowSlot = NexVR_Unreal_StageFrame(1008, dummyColor);
    EXPECT_EQ(overflowSlot, -1);

    // Free the first slot
    NexVR_Unreal_ProcessRenderCommand(claimedSlots[0]);

    // Now a new frame can be staged into the freed slot!
    int reclaimedSlot = NexVR_Unreal_StageFrame(1009, dummyColor);
    EXPECT_EQ(reclaimedSlot, claimedSlots[0]);

    // Cleanup all slots
    for (size_t i = 1; i < claimedSlots.size(); ++i) {
        NexVR_Unreal_ProcessRenderCommand(claimedSlots[i]);
    }
    NexVR_Unreal_ProcessRenderCommand(reclaimedSlot);
}

TEST_F(UnrealBridgeTest, ProcessRenderCommandInvalidSlotIgnored) {
    // Negative or out of bounds slots should be safely ignored
    EXPECT_NO_FATAL_FAILURE(NexVR_Unreal_ProcessRenderCommand(-1));
    EXPECT_NO_FATAL_FAILURE(NexVR_Unreal_ProcessRenderCommand(8));
    EXPECT_NO_FATAL_FAILURE(NexVR_Unreal_ProcessRenderCommand(999));
}

TEST_F(UnrealBridgeTest, GetDeviceNullPointersSafe) {
    void* dev = nullptr;
    void* ctx = nullptr;
    EXPECT_EQ(NexVR_Unreal_GetDeviceFromTexture(nullptr, &dev, &ctx), 0);
    EXPECT_EQ(NexVR_Unreal_GetDeviceFromResourceDX12(nullptr, &dev), 0);

    EXPECT_NO_FATAL_FAILURE(NexVR_Unreal_ReleaseDeviceHandles(nullptr, nullptr));
    EXPECT_NO_FATAL_FAILURE(NexVR_Unreal_ReleaseDeviceHandleDX12(nullptr));
}

TEST_F(UnrealBridgeTest, InputAndHapticsNullSessionSafe) {
    NexVR_ControllerState state{};
    state.structSize = sizeof(NexVR_ControllerState);

    EXPECT_EQ(NexVR_Unreal_SyncInput(), 0);
    EXPECT_EQ(NexVR_Unreal_GetControllerState(0, &state), 0);
    EXPECT_EQ(NexVR_Unreal_GetControllerState(1, &state), 0);
    EXPECT_EQ(NexVR_Unreal_GetControllerState(0, nullptr), 0);
    EXPECT_EQ(NexVR_Unreal_TriggerHaptic(0, 50.0f, 160.0f, 0.8f), 0);
    EXPECT_EQ(NexVR_Unreal_StopHaptic(0), 0);
}

TEST_F(UnrealBridgeTest, StereixUnrealBridgeEquivalence) {
    auto dummySession = reinterpret_cast<NexVR_Session>(uintptr_t(0x66665555ULL));
    Stereix_Unreal_Attach(dummySession);

    uint32_t submitted = 10, dropped = 10, failed = 10;
    int32_t lastResult = -99;
    Stereix_Unreal_GetStats(&submitted, &dropped, &failed, &lastResult);

    EXPECT_EQ(submitted, 0u);
    EXPECT_EQ(dropped, 0u);
    EXPECT_EQ(failed, 0u);
    EXPECT_EQ(lastResult, NEXVR_SUCCESS);

    Stereix_Unreal_Detach();
    EXPECT_EQ(Stereix_Unreal_SupportsDepthSubmission(), 0);
}


