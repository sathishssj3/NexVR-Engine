#pragma once

#include <string>
#include <vector>
#include <memory>
#include <onnxruntime_cxx_api.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl.h>
#include <DirectML.h>

namespace vrinject {
namespace ai {

class AiProfiler {
public:
    AiProfiler();
    ~AiProfiler();

    bool Initialize(const std::wstring& moduleDir);
    void DispatchAll();

private:
    bool LoadModel(const std::wstring& modelPath, std::unique_ptr<Ort::Session>& session);
    void RunModel(Ort::Session& session, const std::vector<int64_t>& inputDims, std::vector<float>& dummyData);
    void WaitForQueue();

    std::unique_ptr<Ort::Env> m_env;
    std::unique_ptr<Ort::SessionOptions> m_sessionOptions;
    std::unique_ptr<Ort::MemoryInfo> m_memoryInfo;

    std::unique_ptr<Ort::Session> m_flowSession;
    std::unique_ptr<Ort::Session> m_gazeSession;
    std::unique_ptr<Ort::Session> m_comfortSession;
    
    std::vector<float> m_dummyFlowData;
    std::vector<float> m_dummyGazeData;
    std::vector<float> m_dummyComfortData;

    Microsoft::WRL::ComPtr<ID3D12Device> m_d3d12Device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;
    Microsoft::WRL::ComPtr<ID3D12QueryHeap> m_queryHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_readbackBuffer;
    Microsoft::WRL::ComPtr<IDMLDevice> m_dmlDevice;
    
    Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
    uint64_t m_fenceValue = 0;
    HANDLE m_fenceEvent = nullptr;
};

} // namespace ai
} // namespace vrinject
