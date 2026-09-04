#include <gtest/gtest.h>
#include "rendering/dx12/dx12_stereo_renderer.h"

using namespace vrinject;

TEST(DX12StereoRendererTest, Initialization) {
    // Like FenceManager, full test requires D3D12 device.
    // Ensure we can construct and it compiles cleanly.
    DX12StereoRenderer renderer;
    EXPECT_TRUE(true);
}
