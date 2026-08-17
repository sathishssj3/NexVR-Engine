#include <gtest/gtest.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <cmath>

#include "mock_openxr_runtime.h"
#include "core/frame_coordinator.h"
#include "rendering/dx11/dx11_graphics_backend.h"
#include "heuristics/camera_lock_manager.h"
#include "heuristics/depth_lock_manager.h"
#include "heuristics/candidate_collector.h"
#include "rendering/vulkan/vulkan_depth_candidate_collector.h"
#include "core/subsystem_context.h"
#include "core/logger.h"

using namespace vrinject;
using namespace vrinject::harness;

class ConsoleLogger : public vrinject::ILogger {
public:
    void Init(const std::string& path) override {}
    void Shutdown() override {}
    void Log(Level level, const char* file, int line, const char* format, ...) override {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        printf("\n");
        va_end(args);
    }
};

class HeadlessHarnessTest : public ::testing::Test {
protected:
    void SetUp() override {
        vrinject::SubsystemContext::Get().Initialize(std::make_shared<ConsoleLogger>(), nullptr);
        MockOpenXRRuntime::Initialize();

        UINT createDeviceFlags = 0;
        D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
        D3D_FEATURE_LEVEL featureLevel;

        HRESULT hr = D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags,
            featureLevels, 1, D3D11_SDK_VERSION, &m_device, &featureLevel, &m_context);

        if (FAILED(hr)) {
            hr = D3D11CreateDevice(
                nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags,
                featureLevels, 1, D3D11_SDK_VERSION, &m_device, &featureLevel, &m_context);
        }
        ASSERT_TRUE(SUCCEEDED(hr));

        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = 1024;
        desc.Height = 1024;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        hr = m_device->CreateTexture2D(&desc, nullptr, &m_backBuffer);
        ASSERT_TRUE(SUCCEEDED(hr));

        D3D11_TEXTURE2D_DESC depthDesc = {};
        depthDesc.Width = 1024;
        depthDesc.Height = 1024;
        depthDesc.MipLevels = 1;
        depthDesc.ArraySize = 1;
        depthDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        depthDesc.SampleDesc.Count = 1;
        depthDesc.Usage = D3D11_USAGE_DEFAULT;
        depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
        hr = m_device->CreateTexture2D(&depthDesc, nullptr, &m_depthBuffer);
        ASSERT_TRUE(SUCCEEDED(hr));
    }

    void TearDown() override {
        MockOpenXRRuntime::Shutdown();
        m_depthBuffer.Reset();
        m_backBuffer.Reset();
        m_context.Reset();
        m_device.Reset();
    }

    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_backBuffer;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_depthBuffer;
};

TEST_F(HeadlessHarnessTest, Stages4To8EndToEnd) {
    MockOpenXRRuntime::SetNextPose({{0.0f, 0.7071068f, 0.0f, 0.7071068f}, {0.0f, 0.0f, 0.0f}}); // 90 deg yaw left

    RenderFrameSnapshot snapshot;
    snapshot.backend = GraphicsBackend::DX11;
    snapshot.nativeDevice = m_device.Get();
    snapshot.nativeContext = m_context.Get();
    snapshot.nativeSwapchain = (void*)0x9999;
    snapshot.backBuffer = m_backBuffer.Get();
    snapshot.width = 1024;
    snapshot.height = 1024;
    snapshot.format = DXGI_FORMAT_R8G8B8A8_UNORM;

    void* fakeDepth = m_depthBuffer.Get();

    for (int frame = 1; frame <= 10; ++frame) {
        CameraCandidate cam;
        cam.valid = true;
        cam.confidence = 1.0f;
        cam.id = 1;
        cam.temporalScore = 1.0f;
        cam.updateCount = 100;
        
        cam.view = {};
        cam.view.m[0][0] = 1.0f; cam.view.m[1][1] = 1.0f; cam.view.m[2][2] = 1.0f; cam.view.m[3][3] = 1.0f;
        cam.projection = cam.view; 
        cam.viewProjection = cam.view;
        SubsystemContext::Get().GetCandidateCollector()->SubmitCandidate(cam);

        SubsystemContext::Get().GetDepthCandidateCollector()->OnDepthSurfaceCreated(fakeDepth, 1024, 1024, 40, 1, 1, 1, 0); // 40 = DXGI_FORMAT_D32_FLOAT
        for (int i = 0; i < 10; ++i) {
            SubsystemContext::Get().GetDepthCandidateCollector()->OnClearDepthStencilView(fakeDepth, 1.0f);
            SubsystemContext::Get().GetDepthCandidateCollector()->OnOMSetRenderTargets(fakeDepth);
        }

        // Supply context to camera manager so snapshot is valid
        SubsystemContext::Get().GetCameraLockManager()->SetResourceContext(snapshot.BackBufferIdentity());

        // Drive the frame
        ScopedFrame scopedFrame(*SubsystemContext::Get().GetFrameCoordinator(), snapshot);
        printf("Frame %d ended, views.size() = %zu\n", frame, MockOpenXRRuntime::GetSubmittedViews().size());
    }

    // Verify OpenXR received the views
    const auto& views = MockOpenXRRuntime::GetSubmittedViews();
    printf("Final views size: %zu\n", views.size());
    ASSERT_EQ(views.size(), 2);

    // Left eye rotation validation
    // Base camera was identity. Headset is rotated 90 deg yaw left (+Y axis).
    // The view matrix should have the forward vector (-Z) rotated to point right (+X).
    // Actually, view matrix transforms world TO view space.
    // So the world's -Z vector (0,0,-1) in view space is what?
    // The view matrix's forward vector (m[2][0], m[2][1], m[2][2]) is the Z axis of the camera in world space.
    // If we rotated left, the camera is looking down the -X axis.
    // Let's assert the views are not identity.
    EXPECT_NE(views[0].pose.orientation.y, 0.0f);
    EXPECT_FLOAT_EQ(views[0].pose.orientation.y, 0.7071068f);
}
