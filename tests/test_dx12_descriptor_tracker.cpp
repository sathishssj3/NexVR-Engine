#include <gtest/gtest.h>
#include "../src/core/dx12_descriptor_tracker.h"
#include <d3d12.h>
#include <gmock/gmock.h>

using namespace vrinject;

class MockD3D12Resource : public ID3D12Resource {
public:
    D3D12_RESOURCE_DESC desc{};
    MockD3D12Resource() {
        desc.Width = 1920;
        desc.Height = 1080;
        desc.Format = DXGI_FORMAT_D32_FLOAT;
        desc.SampleDesc.Count = 1;
        desc.MipLevels = 1;
        desc.DepthOrArraySize = 1;
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    }

    MOCK_METHOD(HRESULT, QueryInterface, (REFIID riid, void **ppvObject), (override, stdcall));
    MOCK_METHOD(ULONG, AddRef, (), (override, stdcall));
    MOCK_METHOD(ULONG, Release, (), (override, stdcall));

    MOCK_METHOD(HRESULT, GetPrivateData, (REFGUID guid, UINT *pDataSize, void *pData), (override, stdcall));
    MOCK_METHOD(HRESULT, SetPrivateData, (REFGUID guid, UINT DataSize, const void *pData), (override, stdcall));
    MOCK_METHOD(HRESULT, SetPrivateDataInterface, (REFGUID guid, const IUnknown *pData), (override, stdcall));
    MOCK_METHOD(HRESULT, SetName, (LPCWSTR Name), (override, stdcall));
    MOCK_METHOD(HRESULT, GetDevice, (REFIID riid, void **ppvDevice), (override, stdcall));
    
    // ID3D12Resource
    MOCK_METHOD(HRESULT, Map, (UINT Subresource, const D3D12_RANGE *pReadRange, void **ppData), (override, stdcall));
    MOCK_METHOD(void, Unmap, (UINT Subresource, const D3D12_RANGE *pWrittenRange), (override, stdcall));
    MOCK_METHOD(D3D12_RESOURCE_DESC, GetDesc, (), (override, stdcall));
    MOCK_METHOD(D3D12_GPU_VIRTUAL_ADDRESS, GetGPUVirtualAddress, (), (override, stdcall));
    MOCK_METHOD(HRESULT, WriteToSubresource, (UINT DstSubresource, const D3D12_BOX *pDstBox, const void *pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch), (override, stdcall));
    MOCK_METHOD(HRESULT, ReadFromSubresource, (void *pDstData, UINT DstRowPitch, UINT DstDepthPitch, UINT SrcSubresource, const D3D12_BOX *pSrcBox), (override, stdcall));
    MOCK_METHOD(HRESULT, GetHeapProperties, (D3D12_HEAP_PROPERTIES *pHeapProperties, D3D12_HEAP_FLAGS *pHeapFlags), (override, stdcall));
};

class Dx12DescriptorTrackerTest : public ::testing::Test {
protected:
    void SetUp() override {
        Dx12DescriptorTracker::Get().Clear();
    }
    void TearDown() override {
        Dx12DescriptorTracker::Get().Clear();
    }
};

TEST_F(Dx12DescriptorTrackerTest, BasicRegistrationAndResolution) {
    MockD3D12Resource resource;
    EXPECT_CALL(resource, GetDesc()).WillRepeatedly(testing::Return(resource.desc));

    D3D12_CPU_DESCRIPTOR_HANDLE handle;
    handle.ptr = 0x1000;

    Dx12DescriptorTracker::Get().RegisterDescriptor(handle, &resource, nullptr);

    GraphicsResourceIdentity id;
    EXPECT_TRUE(Dx12DescriptorTracker::Get().ResolveDescriptor(handle, id));
    EXPECT_EQ(id.backend, GraphicsBackend::DX12);
    EXPECT_EQ(id.nativeHandle, &resource);
    EXPECT_EQ(id.width, 1920);
    EXPECT_EQ(id.height, 1080);
    EXPECT_EQ(id.format, DXGI_FORMAT_D32_FLOAT);
}

TEST_F(Dx12DescriptorTrackerTest, DescriptorCopyChain) {
    MockD3D12Resource resource;
    EXPECT_CALL(resource, GetDesc()).WillRepeatedly(testing::Return(resource.desc));

    D3D12_CPU_DESCRIPTOR_HANDLE handleA{0x1000};
    D3D12_CPU_DESCRIPTOR_HANDLE handleB{0x2000};
    D3D12_CPU_DESCRIPTOR_HANDLE handleC{0x3000};

    Dx12DescriptorTracker::Get().RegisterDescriptor(handleA, &resource, nullptr);
    
    // Copy A -> B
    Dx12DescriptorTracker::Get().CopyDescriptor(handleB, handleA);
    // Copy B -> C
    Dx12DescriptorTracker::Get().CopyDescriptor(handleC, handleB);

    GraphicsResourceIdentity id;
    EXPECT_TRUE(Dx12DescriptorTracker::Get().ResolveDescriptor(handleC, id));
    EXPECT_EQ(id.nativeHandle, &resource);
}

TEST_F(Dx12DescriptorTrackerTest, DescriptorReuseABA) {
    MockD3D12Resource resource1;
    MockD3D12Resource resource2;
    EXPECT_CALL(resource1, GetDesc()).WillRepeatedly(testing::Return(resource1.desc));
    EXPECT_CALL(resource2, GetDesc()).WillRepeatedly(testing::Return(resource2.desc));

    D3D12_CPU_DESCRIPTOR_HANDLE handle{0x1000};

    // Create descriptor for resource 1
    Dx12DescriptorTracker::Get().RegisterDescriptor(handle, &resource1, nullptr);

    GraphicsResourceIdentity id1;
    EXPECT_TRUE(Dx12DescriptorTracker::Get().ResolveDescriptor(handle, id1));
    EXPECT_EQ(id1.nativeHandle, &resource1);

    // Destroy resource 1 (invalidates mapping)
    Dx12DescriptorTracker::Get().OnResourceReleased(&resource1);

    // Should no longer resolve
    EXPECT_FALSE(Dx12DescriptorTracker::Get().ResolveDescriptor(handle, id1));

    // Reuse handle for resource 2
    Dx12DescriptorTracker::Get().RegisterDescriptor(handle, &resource2, nullptr);

    GraphicsResourceIdentity id2;
    EXPECT_TRUE(Dx12DescriptorTracker::Get().ResolveDescriptor(handle, id2));
    EXPECT_EQ(id2.nativeHandle, &resource2);
    
    // ABA verification: generation should be different
    EXPECT_NE(id1.epoch.resourceGeneration, id2.epoch.resourceGeneration);
}

TEST_F(Dx12DescriptorTrackerTest, HeapRecreation) {
    MockD3D12Resource resource;
    EXPECT_CALL(resource, GetDesc()).WillRepeatedly(testing::Return(resource.desc));

    D3D12_CPU_DESCRIPTOR_HANDLE handle1{0x1000};
    D3D12_CPU_DESCRIPTOR_HANDLE handle2{0x1020};
    D3D12_CPU_DESCRIPTOR_HANDLE handle3{0x1040};
    D3D12_CPU_DESCRIPTOR_HANDLE handleOutside{0x3000}; // Different heap

    Dx12DescriptorTracker::Get().RegisterDescriptor(handle1, &resource, nullptr);
    Dx12DescriptorTracker::Get().RegisterDescriptor(handle2, &resource, nullptr);
    Dx12DescriptorTracker::Get().RegisterDescriptor(handle3, &resource, nullptr);
    Dx12DescriptorTracker::Get().RegisterDescriptor(handleOutside, &resource, nullptr);

    // Destroy heap range [0x1000, 0x2000)
    Dx12DescriptorTracker::Get().InvalidateRange(0x1000, 0x2000);

    GraphicsResourceIdentity id;
    EXPECT_FALSE(Dx12DescriptorTracker::Get().ResolveDescriptor(handle1, id));
    EXPECT_FALSE(Dx12DescriptorTracker::Get().ResolveDescriptor(handle2, id));
    EXPECT_FALSE(Dx12DescriptorTracker::Get().ResolveDescriptor(handle3, id));
    
    // handleOutside should survive
    EXPECT_TRUE(Dx12DescriptorTracker::Get().ResolveDescriptor(handleOutside, id));
}

TEST_F(Dx12DescriptorTrackerTest, MultipleDSVs) {
    MockD3D12Resource sceneDepth;
    sceneDepth.desc.Width = 1920; sceneDepth.desc.Height = 1080;
    
    MockD3D12Resource shadowMap;
    shadowMap.desc.Width = 4096; shadowMap.desc.Height = 4096;
    
    MockD3D12Resource uiDepth;
    uiDepth.desc.Width = 800; uiDepth.desc.Height = 600;

    EXPECT_CALL(sceneDepth, GetDesc()).WillRepeatedly(testing::Return(sceneDepth.desc));
    EXPECT_CALL(shadowMap, GetDesc()).WillRepeatedly(testing::Return(shadowMap.desc));
    EXPECT_CALL(uiDepth, GetDesc()).WillRepeatedly(testing::Return(uiDepth.desc));

    D3D12_CPU_DESCRIPTOR_HANDLE handleScene{0x10};
    D3D12_CPU_DESCRIPTOR_HANDLE handleShadow{0x20};
    D3D12_CPU_DESCRIPTOR_HANDLE handleUI{0x30};

    Dx12DescriptorTracker::Get().RegisterDescriptor(handleScene, &sceneDepth, nullptr);
    Dx12DescriptorTracker::Get().RegisterDescriptor(handleShadow, &shadowMap, nullptr);
    Dx12DescriptorTracker::Get().RegisterDescriptor(handleUI, &uiDepth, nullptr);

    GraphicsResourceIdentity id;
    
    Dx12DescriptorTracker::Get().ResolveDescriptor(handleShadow, id);
    EXPECT_EQ(id.width, 4096);
    EXPECT_EQ(id.nativeHandle, &shadowMap);
    
    Dx12DescriptorTracker::Get().ResolveDescriptor(handleScene, id);
    EXPECT_EQ(id.width, 1920);
    EXPECT_EQ(id.nativeHandle, &sceneDepth);
}
