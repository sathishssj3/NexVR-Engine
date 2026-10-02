#include <gtest/gtest.h>
#include "ai/tensor_bridge.h"

using namespace NexVR::AI;

TEST(TensorBridgeTest, InitShutdown) {
    // Tests that creating and destroying TensorBridge doesn't crash
    TensorBridge bridge;
    EXPECT_EQ(bridge.GetDmlExecutionContext(), nullptr);
}
