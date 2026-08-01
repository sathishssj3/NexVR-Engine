#include <gtest/gtest.h>
#include "../src/rendering/dx12_graphics_backend.h"

using namespace vrinject;

TEST(DX12GraphicsBackendTest, Instantiation) {
    // Like FenceManager, full test requires D3D12 device.
    // Ensure we can construct and it compiles cleanly.
    DX12GraphicsBackend backend;
    EXPECT_EQ(backend.GetAPI(), GraphicsBackend::DX12);
    EXPECT_EQ(backend.GetState(), StereoRendererState::UNINITIALIZED);
}
