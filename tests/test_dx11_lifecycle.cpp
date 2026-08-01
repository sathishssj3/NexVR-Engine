#include <gtest/gtest.h>
#include "../src/core/dx11_lifecycle_manager.h"
#include "../src/core/dx11_resource_validator.h"
#include "../src/core/fault_injector.h"
#include <d3d11.h>
#include <wrl/client.h>

using namespace vrinject;

class Dx11LifecycleManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        Dx11LifecycleManager::Get().Reset();
        FaultInjector::Clear();
    }
    void TearDown() override {
        Dx11LifecycleManager::Get().Reset();
        FaultInjector::Clear();
    }

    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
    Microsoft::WRL::ComPtr<IDXGISwapChain> m_swapChain;

    void CreateDummyDx11() {
        DXGI_SWAP_CHAIN_DESC sd = {};
        sd.BufferCount = 2;
        sd.BufferDesc.Width = 100;
        sd.BufferDesc.Height = 100;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.RefreshRate.Numerator = 60;
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        
        WNDCLASSEXA wc = { sizeof(WNDCLASSEXA), CS_HREDRAW | CS_VREDRAW, DefWindowProcA, 0, 0, GetModuleHandleA(nullptr), NULL, NULL, NULL, NULL, "TestDummy", NULL };
        RegisterClassExA(&wc);
        HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "Dummy", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, wc.hInstance, NULL);
        sd.OutputWindow = hwnd;
        sd.SampleDesc.Count = 1;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        D3D_FEATURE_LEVEL featureLevel;
        D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &sd, &m_swapChain, &m_device, &featureLevel, &m_context);
    }
};

TEST_F(Dx11LifecycleManagerTest, InitialStateIsUninitialized) {
    EXPECT_EQ(Dx11LifecycleManager::Get().GetState(), RenderState::UNINITIALIZED);
}

TEST_F(Dx11LifecycleManagerTest, ProcessPresentTransitionsToActive) {
    CreateDummyDx11();
    ASSERT_NE(m_swapChain, nullptr);

    Dx11ContextSnapshot snapshot = Dx11LifecycleManager::Get().ProcessPresent(m_swapChain.Get(), S_OK);
    
    EXPECT_EQ(Dx11LifecycleManager::Get().GetState(), RenderState::RUNNING);
    
    auto validation = Dx11ResourceValidator::Validate(snapshot, Dx11LifecycleManager::Get().GetEpoch());
    EXPECT_EQ(validation.status, Dx11ValidationStatus::VALID);
    EXPECT_EQ(snapshot.device, m_device.Get());
}

TEST_F(Dx11LifecycleManagerTest, FaultInjectorDeviceRemoved) {
    CreateDummyDx11();
    Dx11LifecycleManager::Get().ProcessPresent(m_swapChain.Get(), S_OK);
    EXPECT_EQ(Dx11LifecycleManager::Get().GetState(), RenderState::RUNNING);

    // Inject Device Removed Fault
    FaultInjector::InjectFault(Dx11FaultPoint::PRESENT_DEVICE_REMOVED);
    
    Dx11ContextSnapshot snapshot = Dx11LifecycleManager::Get().ProcessPresent(m_swapChain.Get(), S_OK);
    
    // It should immediately transition to REINITIALIZING after trying to recover
    EXPECT_EQ(Dx11LifecycleManager::Get().GetState(), RenderState::REINITIALIZING);
    
    auto validation = Dx11ResourceValidator::Validate(snapshot, Dx11LifecycleManager::Get().GetEpoch());
    // The snapshot returned during recovery will typically have nulls or be stale
    EXPECT_NE(validation.status, Dx11ValidationStatus::VALID);
}

TEST_F(Dx11LifecycleManagerTest, ReinitializesAfterDeviceRemoved) {
    CreateDummyDx11();
    Dx11LifecycleManager::Get().ProcessPresent(m_swapChain.Get(), S_OK);
    
    // Fail
    FaultInjector::InjectFault(Dx11FaultPoint::PRESENT_DEVICE_REMOVED);
    Dx11LifecycleManager::Get().ProcessPresent(m_swapChain.Get(), S_OK);
    EXPECT_EQ(Dx11LifecycleManager::Get().GetState(), RenderState::REINITIALIZING);

    // Provide a new valid present
    Dx11ContextSnapshot snapshot = Dx11LifecycleManager::Get().ProcessPresent(m_swapChain.Get(), S_OK);
    
    EXPECT_EQ(Dx11LifecycleManager::Get().GetState(), RenderState::RUNNING);
    
    auto validation = Dx11ResourceValidator::Validate(snapshot, Dx11LifecycleManager::Get().GetEpoch());
    EXPECT_EQ(validation.status, Dx11ValidationStatus::VALID);
    EXPECT_EQ(snapshot.epoch.deviceGeneration, 2); // Initial was 1, reinit makes it 2
}

TEST_F(Dx11LifecycleManagerTest, DegradesAfterBudgetExhausted) {
    CreateDummyDx11();
    Dx11LifecycleManager::Get().ProcessPresent(m_swapChain.Get(), S_OK);

    for (int i = 0; i < 4; ++i) {
        // Keep passing DEVICE_REMOVED without S_OK in between
        Dx11LifecycleManager::Get().ProcessPresent(m_swapChain.Get(), DXGI_ERROR_DEVICE_REMOVED);
    }
    
    // We should be degraded now
    EXPECT_EQ(Dx11LifecycleManager::Get().GetState(), RenderState::DEGRADED);

    EXPECT_EQ(Dx11LifecycleManager::Get().GetState(), RenderState::DEGRADED);
}
