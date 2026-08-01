#include <gtest/gtest.h>
#include "../src/rendering/dx12_resource_state_tracker.h"
#include <d3d12.h>
#include <gmock/gmock.h>

using namespace vrinject;

class MockD3D12GraphicsCommandList : public ID3D12GraphicsCommandList {
public:
    MOCK_METHOD(HRESULT, QueryInterface, (REFIID riid, void **ppvObject), (override, stdcall));
    MOCK_METHOD(ULONG, AddRef, (), (override, stdcall));
    MOCK_METHOD(ULONG, Release, (), (override, stdcall));

    MOCK_METHOD(HRESULT, GetPrivateData, (REFGUID guid, UINT *pDataSize, void *pData), (override, stdcall));
    MOCK_METHOD(HRESULT, SetPrivateData, (REFGUID guid, UINT DataSize, const void *pData), (override, stdcall));
    MOCK_METHOD(HRESULT, SetPrivateDataInterface, (REFGUID guid, const IUnknown *pData), (override, stdcall));
    MOCK_METHOD(HRESULT, SetName, (LPCWSTR Name), (override, stdcall));

    MOCK_METHOD(HRESULT, GetDevice, (REFIID riid, void **ppvDevice), (override, stdcall));

    MOCK_METHOD(D3D12_COMMAND_LIST_TYPE, GetType, (), (override, stdcall));
    MOCK_METHOD(HRESULT, Close, (), (override, stdcall));
    MOCK_METHOD(HRESULT, Reset, (ID3D12CommandAllocator *pAllocator, ID3D12PipelineState *pInitialState), (override, stdcall));

    MOCK_METHOD(void, ClearState, (ID3D12PipelineState *pPipelineState), (override, stdcall));
    MOCK_METHOD(void, DrawInstanced, (UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation), (override, stdcall));
    MOCK_METHOD(void, DrawIndexedInstanced, (UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation), (override, stdcall));
    MOCK_METHOD(void, Dispatch, (UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ), (override, stdcall));
    MOCK_METHOD(void, CopyBufferRegion, (ID3D12Resource *pDstBuffer, UINT64 DstOffset, ID3D12Resource *pSrcBuffer, UINT64 SrcOffset, UINT64 NumBytes), (override, stdcall));
    MOCK_METHOD(void, CopyTextureRegion, (const D3D12_TEXTURE_COPY_LOCATION *pDst, UINT DstX, UINT DstY, UINT DstZ, const D3D12_TEXTURE_COPY_LOCATION *pSrc, const D3D12_BOX *pSrcBox), (override, stdcall));
    MOCK_METHOD(void, CopyResource, (ID3D12Resource *pDstResource, ID3D12Resource *pSrcResource), (override, stdcall));
    MOCK_METHOD(void, CopyTiles, (ID3D12Resource *pTiledResource, const D3D12_TILED_RESOURCE_COORDINATE *pTileRegionStartCoordinate, const D3D12_TILE_REGION_SIZE *pTileRegionSize, ID3D12Resource *pBuffer, UINT64 BufferStartOffsetInBytes, D3D12_TILE_COPY_FLAGS Flags), (override, stdcall));
    MOCK_METHOD(void, ResolveSubresource, (ID3D12Resource *pDstResource, UINT DstSubresource, ID3D12Resource *pSrcResource, UINT SrcSubresource, DXGI_FORMAT Format), (override, stdcall));
    MOCK_METHOD(void, IASetPrimitiveTopology, (D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology), (override, stdcall));
    MOCK_METHOD(void, RSSetViewports, (UINT NumViewports, const D3D12_VIEWPORT *pViewports), (override, stdcall));
    MOCK_METHOD(void, RSSetScissorRects, (UINT NumRects, const D3D12_RECT *pRects), (override, stdcall));
    MOCK_METHOD(void, OMSetBlendFactor, (const FLOAT BlendFactor[4]), (override, stdcall));
    MOCK_METHOD(void, OMSetStencilRef, (UINT StencilRef), (override, stdcall));
    MOCK_METHOD(void, SetPipelineState, (ID3D12PipelineState *pPipelineState), (override, stdcall));
    
    // The important one
    MOCK_METHOD(void, ResourceBarrier, (UINT NumBarriers, const D3D12_RESOURCE_BARRIER *pBarriers), (override, stdcall));

    MOCK_METHOD(void, ExecuteBundle, (ID3D12GraphicsCommandList *pCommandList), (override, stdcall));
    MOCK_METHOD(void, SetDescriptorHeaps, (UINT NumDescriptorHeaps, ID3D12DescriptorHeap *const *ppDescriptorHeaps), (override, stdcall));
    MOCK_METHOD(void, SetComputeRootSignature, (ID3D12RootSignature *pRootSignature), (override, stdcall));
    MOCK_METHOD(void, SetGraphicsRootSignature, (ID3D12RootSignature *pRootSignature), (override, stdcall));
    MOCK_METHOD(void, SetComputeRootDescriptorTable, (UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor), (override, stdcall));
    MOCK_METHOD(void, SetGraphicsRootDescriptorTable, (UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor), (override, stdcall));
    MOCK_METHOD(void, SetComputeRoot32BitConstant, (UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues), (override, stdcall));
    MOCK_METHOD(void, SetGraphicsRoot32BitConstant, (UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues), (override, stdcall));
    MOCK_METHOD(void, SetComputeRoot32BitConstants, (UINT RootParameterIndex, UINT Num32BitValuesToSet, const void *pSrcData, UINT DestOffsetIn32BitValues), (override, stdcall));
    MOCK_METHOD(void, SetGraphicsRoot32BitConstants, (UINT RootParameterIndex, UINT Num32BitValuesToSet, const void *pSrcData, UINT DestOffsetIn32BitValues), (override, stdcall));
    MOCK_METHOD(void, SetComputeRootConstantBufferView, (UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation), (override, stdcall));
    MOCK_METHOD(void, SetGraphicsRootConstantBufferView, (UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation), (override, stdcall));
    MOCK_METHOD(void, SetComputeRootShaderResourceView, (UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation), (override, stdcall));
    MOCK_METHOD(void, SetGraphicsRootShaderResourceView, (UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation), (override, stdcall));
    MOCK_METHOD(void, SetComputeRootUnorderedAccessView, (UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation), (override, stdcall));
    MOCK_METHOD(void, SetGraphicsRootUnorderedAccessView, (UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation), (override, stdcall));
    MOCK_METHOD(void, IASetIndexBuffer, (const D3D12_INDEX_BUFFER_VIEW *pView), (override, stdcall));
    MOCK_METHOD(void, IASetVertexBuffers, (UINT StartSlot, UINT NumViews, const D3D12_VERTEX_BUFFER_VIEW *pViews), (override, stdcall));
    MOCK_METHOD(void, SOSetTargets, (UINT StartSlot, UINT NumViews, const D3D12_STREAM_OUTPUT_BUFFER_VIEW *pViews), (override, stdcall));
    MOCK_METHOD(void, OMSetRenderTargets, (UINT NumRenderTargetDescriptors, const D3D12_CPU_DESCRIPTOR_HANDLE *pRenderTargetDescriptors, BOOL RTsSingleHandleToDescriptorRange, const D3D12_CPU_DESCRIPTOR_HANDLE *pDepthStencilDescriptor), (override, stdcall));
    MOCK_METHOD(void, ClearDepthStencilView, (D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView, D3D12_CLEAR_FLAGS ClearFlags, FLOAT Depth, UINT8 Stencil, UINT NumRects, const D3D12_RECT *pRects), (override, stdcall));
    MOCK_METHOD(void, ClearRenderTargetView, (D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView, const FLOAT ColorRGBA[4], UINT NumRects, const D3D12_RECT *pRects), (override, stdcall));
    MOCK_METHOD(void, ClearUnorderedAccessViewUint, (D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap, D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle, ID3D12Resource *pResource, const UINT Values[4], UINT NumRects, const D3D12_RECT *pRects), (override, stdcall));
    MOCK_METHOD(void, ClearUnorderedAccessViewFloat, (D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap, D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle, ID3D12Resource *pResource, const FLOAT Values[4], UINT NumRects, const D3D12_RECT *pRects), (override, stdcall));
    MOCK_METHOD(void, DiscardResource, (ID3D12Resource *pResource, const D3D12_DISCARD_REGION *pRegion), (override, stdcall));
    MOCK_METHOD(void, BeginQuery, (ID3D12QueryHeap *pQueryHeap, D3D12_QUERY_TYPE Type, UINT Index), (override, stdcall));
    MOCK_METHOD(void, EndQuery, (ID3D12QueryHeap *pQueryHeap, D3D12_QUERY_TYPE Type, UINT Index), (override, stdcall));
    MOCK_METHOD(void, ResolveQueryData, (ID3D12QueryHeap *pQueryHeap, D3D12_QUERY_TYPE Type, UINT StartIndex, UINT NumQueries, ID3D12Resource *pDestinationBuffer, UINT64 AlignedDestinationBufferOffset), (override, stdcall));
    MOCK_METHOD(void, SetPredication, (ID3D12Resource *pBuffer, UINT64 AlignedBufferOffset, D3D12_PREDICATION_OP Operation), (override, stdcall));
    MOCK_METHOD(void, SetMarker, (UINT Metadata, const void *pData, UINT Size), (override, stdcall));
    MOCK_METHOD(void, BeginEvent, (UINT Metadata, const void *pData, UINT Size), (override, stdcall));
    MOCK_METHOD(void, EndEvent, (), (override, stdcall));
    MOCK_METHOD(void, ExecuteIndirect, (ID3D12CommandSignature *pCommandSignature, UINT MaxCommandCount, ID3D12Resource *pArgumentBuffer, UINT64 ArgumentBufferOffset, ID3D12Resource *pCountBuffer, UINT64 CountBufferOffset), (override, stdcall));
};

class DX12ResourceStateTrackerTest : public ::testing::Test {
protected:
    DX12ResourceStateTracker tracker;

    void SetUp() override {
        tracker.Clear();
    }
    
    void TearDown() override {
        tracker.Clear();
    }
};

TEST_F(DX12ResourceStateTrackerTest, InitialStateAndTransition) {
    ID3D12Resource* mockResource = reinterpret_cast<ID3D12Resource*>(0xDEADBEEF);
    
    tracker.RegisterResource(mockResource, D3D12_RESOURCE_STATE_COPY_DEST);
    
    // Same state, should return false (redundant)
    EXPECT_FALSE(tracker.TransitionResource(mockResource, D3D12_RESOURCE_STATE_COPY_DEST));
    EXPECT_TRUE(tracker.GetPendingBarriers().empty());
    
    // New state, should return true
    EXPECT_TRUE(tracker.TransitionResource(mockResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
    
    const auto& barriers = tracker.GetPendingBarriers();
    ASSERT_EQ(barriers.size(), 1);
    EXPECT_EQ(barriers[0].Type, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
    EXPECT_EQ(barriers[0].Transition.pResource, mockResource);
    EXPECT_EQ(barriers[0].Transition.StateBefore, D3D12_RESOURCE_STATE_COPY_DEST);
    EXPECT_EQ(barriers[0].Transition.StateAfter, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}

TEST_F(DX12ResourceStateTrackerTest, UnknownResourceDefaultsToCommon) {
    ID3D12Resource* mockResource = reinterpret_cast<ID3D12Resource*>(0xBEEFCAFE);
    
    EXPECT_TRUE(tracker.TransitionResource(mockResource, D3D12_RESOURCE_STATE_PRESENT));
    
    const auto& barriers = tracker.GetPendingBarriers();
    ASSERT_EQ(barriers.size(), 1);
    EXPECT_EQ(barriers[0].Transition.StateBefore, D3D12_RESOURCE_STATE_COMMON);
}

TEST_F(DX12ResourceStateTrackerTest, FlushBarriersClearsListAndCallsAPI) {
    ID3D12Resource* mockResource = reinterpret_cast<ID3D12Resource*>(0x11112222);
    tracker.TransitionResource(mockResource, D3D12_RESOURCE_STATE_RENDER_TARGET);
    
    MockD3D12GraphicsCommandList mockCmdList;
    
    EXPECT_CALL(mockCmdList, ResourceBarrier(1, ::testing::_)).Times(1);
    
    tracker.FlushBarriers(&mockCmdList);
    EXPECT_TRUE(tracker.GetPendingBarriers().empty());
}
