// test_unity_bridge.cpp — Functional and concurrency tests for Unity Bridge (D3D11 and D3D12)

#include <gtest/gtest.h>
#include <d3d11.h>
#include <d3d12.h>
#include <vector>

#include "nexvr_sdk.h"
#include "nexvr_unity_bridge.h"

class UnityBridgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        NexVR_Unity_Detach();
    }

    void TearDown() override {
        NexVR_Unity_Detach();
    }
};

TEST_F(UnityBridgeTest, AttachAndDetachLifecycle) {
    auto dummySession = reinterpret_cast<NexVR_Session>(uintptr_t(0x12345678ULL));
    NexVR_Unity_Attach(dummySession);

    uint32_t submitted = 10, dropped = 10, failed = 10;
    int32_t lastResult = -99;
    NexVR_Unity_GetStats(&submitted, &dropped, &failed, &lastResult);

    EXPECT_EQ(submitted, 0u);
    EXPECT_EQ(dropped, 0u);
    EXPECT_EQ(failed, 0u);
    EXPECT_EQ(lastResult, NEXVR_SUCCESS);

    NexVR_Unity_Detach();
    EXPECT_EQ(NexVR_Unity_SupportsDepthSubmission(), 0);
}

TEST_F(UnityBridgeTest, RenderEventFuncNotNull) {
    UnityRenderingEvent renderEvent = NexVR_Unity_GetRenderEventFunc();
    EXPECT_NE(renderEvent, nullptr);
}

TEST_F(UnityBridgeTest, NullTextureRejected) {
    int slot = NexVR_Unity_StageFrame(1001, nullptr);
    EXPECT_EQ(slot, -1);

    slot = NexVR_Unity_StageFrameDX12(1002, nullptr, 0x4);
    EXPECT_EQ(slot, -1);

    slot = NexVR_Unity_StageFrameWithDepthDX12(1003, nullptr, 0x4, nullptr, 0, 0.1f, 100.0f, 1);
    EXPECT_EQ(slot, -1);
}

TEST_F(UnityBridgeTest, StageFrameDX11Success) {
    UnityRenderingEvent renderEvent = NexVR_Unity_GetRenderEventFunc();
    auto dummyColor = reinterpret_cast<void*>(uintptr_t(0xAAAA0000ULL));
    int slot = NexVR_Unity_StageFrame(101, dummyColor);
    EXPECT_GE(slot, 0);
    EXPECT_LT(slot, 8);

    // Consume slot via renderEvent
    renderEvent(slot);
}

TEST_F(UnityBridgeTest, StageFrameWithDepthDX11Success) {
    UnityRenderingEvent renderEvent = NexVR_Unity_GetRenderEventFunc();
    auto dummyColor = reinterpret_cast<void*>(uintptr_t(0xAAAA0000ULL));
    auto dummyDepth = reinterpret_cast<void*>(uintptr_t(0xBBBB0000ULL));
    int slot = NexVR_Unity_StageFrameWithDepth(102, dummyColor, dummyDepth, 0.05f, 1000.0f, 1);
    EXPECT_GE(slot, 0);
    EXPECT_LT(slot, 8);

    renderEvent(slot);
}

TEST_F(UnityBridgeTest, StageFrameDX12Success) {
    UnityRenderingEvent renderEvent = NexVR_Unity_GetRenderEventFunc();
    auto dummyColor = reinterpret_cast<void*>(uintptr_t(0xCCCC0000ULL));
    uint32_t colorState = 0x4; // D3D12_RESOURCE_STATE_RENDER_TARGET
    int slot = NexVR_Unity_StageFrameDX12(201, dummyColor, colorState);
    EXPECT_GE(slot, 0);
    EXPECT_LT(slot, 8);

    renderEvent(slot);
}

TEST_F(UnityBridgeTest, StageFrameWithDepthDX12Success) {
    UnityRenderingEvent renderEvent = NexVR_Unity_GetRenderEventFunc();
    auto dummyColor = reinterpret_cast<void*>(uintptr_t(0xCCCC0000ULL));
    auto dummyDepth = reinterpret_cast<void*>(uintptr_t(0xDDDD0000ULL));
    uint32_t colorState = 0x4;
    uint32_t depthState = 0x20;
    int slot = NexVR_Unity_StageFrameWithDepthDX12(202, dummyColor, colorState,
                                                  dummyDepth, depthState,
                                                  0.05f, 1000.0f, 1);
    EXPECT_GE(slot, 0);
    EXPECT_LT(slot, 8);

    renderEvent(slot);
}

TEST_F(UnityBridgeTest, RingBufferExhaustionAndReclaim) {
    UnityRenderingEvent renderEvent = NexVR_Unity_GetRenderEventFunc();
    auto dummyColor = reinterpret_cast<void*>(uintptr_t(0xEEEE0000ULL));

    std::vector<int> claimedSlots;
    for (int i = 0; i < 8; ++i) {
        int slot = NexVR_Unity_StageFrame(1000 + i, dummyColor);
        EXPECT_EQ(slot, i);
        claimedSlots.push_back(slot);
    }

    // 9th frame rejected
    int overflowSlot = NexVR_Unity_StageFrame(1008, dummyColor);
    EXPECT_EQ(overflowSlot, -1);

    // Free first slot
    renderEvent(claimedSlots[0]);

    // Reclaim
    int reclaimedSlot = NexVR_Unity_StageFrame(1009, dummyColor);
    EXPECT_EQ(reclaimedSlot, claimedSlots[0]);

    // Cleanup remaining
    for (size_t i = 1; i < claimedSlots.size(); ++i) {
        renderEvent(claimedSlots[i]);
    }
    renderEvent(reclaimedSlot);
}

TEST_F(UnityBridgeTest, OnRenderEventInvalidSlotIgnored) {
    UnityRenderingEvent renderEvent = NexVR_Unity_GetRenderEventFunc();
    EXPECT_NO_FATAL_FAILURE(renderEvent(-1));
    EXPECT_NO_FATAL_FAILURE(renderEvent(8));
    EXPECT_NO_FATAL_FAILURE(renderEvent(999));
}

TEST_F(UnityBridgeTest, GetDeviceNullPointersSafe) {
    void* dev = nullptr;
    void* ctx = nullptr;
    EXPECT_EQ(NexVR_Unity_GetDeviceFromTexture(nullptr, &dev, &ctx), 0);
    EXPECT_EQ(NexVR_Unity_GetDeviceFromResourceDX12(nullptr, &dev), 0);

    EXPECT_NO_FATAL_FAILURE(NexVR_Unity_ReleaseDeviceHandles(nullptr, nullptr));
    EXPECT_NO_FATAL_FAILURE(NexVR_Unity_ReleaseDeviceHandleDX12(nullptr));
}

TEST_F(UnityBridgeTest, InputAndHapticsNullSessionSafe) {
    // When no session is attached:
    EXPECT_EQ(NexVR_Unity_SyncInput(), NEXVR_ERROR_INVALID_SESSION);

    NexVR_ControllerState state{};
    state.structSize = sizeof(NexVR_ControllerState);
    EXPECT_EQ(NexVR_Unity_GetControllerState(NEXVR_HAND_LEFT, &state), NEXVR_ERROR_INVALID_SESSION);
    EXPECT_EQ(NexVR_Unity_GetControllerState(NEXVR_HAND_LEFT, nullptr), NEXVR_ERROR_INVALID_ARGUMENT);

    NexVR_HapticFeedback haptic{};
    haptic.structSize = sizeof(NexVR_HapticFeedback);
    haptic.durationMs = 100.0f;
    haptic.amplitude = 0.5f;
    EXPECT_EQ(NexVR_Unity_TriggerHaptic(NEXVR_HAND_RIGHT, &haptic), NEXVR_ERROR_INVALID_SESSION);
    EXPECT_EQ(NexVR_Unity_TriggerHaptic(NEXVR_HAND_RIGHT, nullptr), NEXVR_ERROR_INVALID_ARGUMENT);

    EXPECT_EQ(NexVR_Unity_StopHaptic(NEXVR_HAND_RIGHT), NEXVR_ERROR_INVALID_SESSION);
}

TEST_F(UnityBridgeTest, StereixUnityBridgeEquivalence) {
    auto dummySession = reinterpret_cast<NexVR_Session>(uintptr_t(0x55554444ULL));
    Stereix_Unity_Attach(dummySession);

    uint32_t submitted = 10, dropped = 10, failed = 10;
    int32_t lastResult = -99;
    Stereix_Unity_GetStats(&submitted, &dropped, &failed, &lastResult);

    EXPECT_EQ(submitted, 0u);
    EXPECT_EQ(dropped, 0u);
    EXPECT_EQ(failed, 0u);
    EXPECT_EQ(lastResult, NEXVR_SUCCESS);

    Stereix_Unity_Detach();
    EXPECT_EQ(Stereix_Unity_SupportsDepthSubmission(), 0);

    UnityRenderingEvent renderEvent = Stereix_Unity_GetRenderEventFunc();
    EXPECT_NE(renderEvent, nullptr);
}


