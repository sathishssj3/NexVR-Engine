#include "ai_profiler.h"
#include "../core/logger.h"
#include <dml_provider_factory.h>
#include <iostream>
#include <chrono>

namespace vrinject {
namespace ai {

AiProfiler::AiProfiler() {
}

AiProfiler::~AiProfiler() {
    if (m_fenceEvent) {
        CloseHandle(m_fenceEvent);
    }
}

void AiProfiler::WaitForQueue() {
    if (!m_commandQueue || !m_fence) return;
    
    m_fenceValue++;
    m_commandQueue->Signal(m_fence.Get(), m_fenceValue);
    
    if (m_fence->GetCompletedValue() < m_fenceValue) {
        m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent);
        ::WaitForSingleObject(m_fenceEvent, INFINITE);
    }
}

bool AiProfiler::Initialize(const std::wstring& moduleDir) {
    try {
        // Initialize ONNX Runtime environment
        m_env = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "VRInject_AI_Profiler");
        
        // Initialize D3D12 and DML for GPU profiling
        if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_d3d12Device)))) {
            LOG_ERROR("Failed to create D3D12 device");
            return false;
        }

        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        if (FAILED(m_d3d12Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)))) {
            LOG_ERROR("Failed to create D3D12 Command Queue");
            return false;
        }
        
        m_d3d12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator));
        m_d3d12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_commandList));
        m_commandList->Close();
        
        m_d3d12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        
        D3D12_QUERY_HEAP_DESC queryHeapDesc = {};
        queryHeapDesc.Count = 10;
        queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
        m_d3d12Device->CreateQueryHeap(&queryHeapDesc, IID_PPV_ARGS(&m_queryHeap));
        
        D3D12_HEAP_PROPERTIES heapProps = { D3D12_HEAP_TYPE_READBACK, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };
        D3D12_RESOURCE_DESC bufferDesc = { D3D12_RESOURCE_DIMENSION_BUFFER, 0, 10 * sizeof(uint64_t), 1, 1, 1, DXGI_FORMAT_UNKNOWN, {1, 0}, D3D12_TEXTURE_LAYOUT_ROW_MAJOR, D3D12_RESOURCE_FLAG_NONE };
        m_d3d12Device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_readbackBuffer));

        HMODULE dmlDll = LoadLibraryW(L"directml.dll");
        if (!dmlDll) {
            LOG_ERROR("Failed to load directml.dll");
            return false;
        }
        
        auto pfnDMLCreateDevice = reinterpret_cast<decltype(&DMLCreateDevice)>(GetProcAddress(dmlDll, "DMLCreateDevice"));
        if (!pfnDMLCreateDevice) {
            LOG_ERROR("Failed to get DMLCreateDevice from directml.dll");
            return false;
        }

        if (FAILED(pfnDMLCreateDevice(m_d3d12Device.Get(), DML_CREATE_DEVICE_FLAG_NONE, IID_PPV_ARGS(&m_dmlDevice)))) {
            LOG_ERROR("Failed to create DML device");
            return false;
        }

        m_sessionOptions = std::make_unique<Ort::SessionOptions>();
        m_sessionOptions->SetIntraOpNumThreads(1);
        m_sessionOptions->SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        // Add DirectML execution provider using DML1
        const OrtApi& ortApi = Ort::GetApi();
        const OrtDmlApi* dmlApi = nullptr;
        const void* dmlApiVoid = nullptr;
        ortApi.GetExecutionProviderApi("DML", ORT_API_VERSION, &dmlApiVoid);
        dmlApi = reinterpret_cast<const OrtDmlApi*>(dmlApiVoid);

        if (dmlApi) {
            OrtStatus* status = dmlApi->SessionOptionsAppendExecutionProvider_DML1(
                static_cast<OrtSessionOptions*>(*m_sessionOptions), 
                m_dmlDevice.Get(), 
                m_commandQueue.Get()
            );
            if (status != nullptr) {
                LOG_ERROR("Failed to append DML1 execution provider");
                ortApi.ReleaseStatus(status);
            } else {
                LOG_INFO("Successfully appended DML1 execution provider");
            }
        } else {
            LOG_ERROR("Failed to get DML execution provider API");
        }

        m_memoryInfo = std::make_unique<Ort::MemoryInfo>(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault));
        
        // Load the 3 dummy models
        std::wstring flowModel = moduleDir + L"\\models\\gated_conv_flow.onnx";
        std::wstring gazeModel = moduleDir + L"\\models\\gaze_lstm.onnx";
        std::wstring comfortModel = moduleDir + L"\\models\\comfort_mlp.onnx";
        
        if (!LoadModel(flowModel, m_flowSession)) {
            LOG_ERROR("Failed to load flow model: %ls", flowModel.c_str());
            return false;
        }
        if (!LoadModel(gazeModel, m_gazeSession)) {
            LOG_ERROR("Failed to load gaze model: %ls", gazeModel.c_str());
            return false;
        }
        if (!LoadModel(comfortModel, m_comfortSession)) {
            LOG_ERROR("Failed to load comfort model: %ls", comfortModel.c_str());
            return false;
        }
        
        // Prepare dummy data
        m_dummyFlowData.resize(1 * 2 * 256 * 256, 0.5f);
        m_dummyGazeData.resize(1 * 10 * 6, 0.5f);
        m_dummyComfortData.resize(1 * 64, 0.5f);

        LOG_INFO("AiProfiler initialized successfully with all dummy models.");
        return true;
    } catch (const Ort::Exception& e) {
        LOG_ERROR("ONNX Runtime initialization failed: %s", e.what());
        return false;
    }
}

bool AiProfiler::LoadModel(const std::wstring& modelPath, std::unique_ptr<Ort::Session>& session) {
    try {
        session = std::make_unique<Ort::Session>(*m_env, modelPath.c_str(), *m_sessionOptions);
        return true;
    } catch (const Ort::Exception& e) {
        LOG_ERROR("Failed to load model %ls: %s", modelPath.c_str(), e.what());
        return false;
    }
}

void AiProfiler::RunModel(Ort::Session& session, const std::vector<int64_t>& inputDims, std::vector<float>& dummyData) {
    try {
        size_t inputTensorSize = dummyData.size();
        Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
            *m_memoryInfo, dummyData.data(), inputTensorSize, inputDims.data(), inputDims.size());

        const char* inputNames[] = { "input" };
        const char* outputNames[] = { "output" };

        session.Run(Ort::RunOptions{nullptr}, inputNames, &inputTensor, 1, outputNames, 1);
    } catch (const Ort::Exception& e) {
        LOG_ERROR("Inference failed: %s", e.what());
    }
}

void AiProfiler::DispatchAll() {
    if (!m_flowSession || !m_gazeSession || !m_comfortSession || !m_commandQueue) return;
    
    // Auto helper to record timestamp
    auto RecordTimestamp = [&](uint32_t index) {
        m_commandAllocator->Reset();
        m_commandList->Reset(m_commandAllocator.Get(), nullptr);
        m_commandList->EndQuery(m_queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, index);
        m_commandList->Close();
        ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
        m_commandQueue->ExecuteCommandLists(1, ppCommandLists);
    };

    RecordTimestamp(0);
    
    auto t0 = std::chrono::high_resolution_clock::now();
    // Optical Flow: [1, 2, 256, 256]
    RunModel(*m_flowSession, {1, 2, 256, 256}, m_dummyFlowData);
    
    RecordTimestamp(1);
    
    auto t1 = std::chrono::high_resolution_clock::now();
    // Gaze: [1, 10, 6]
    RunModel(*m_gazeSession, {1, 10, 6}, m_dummyGazeData);
    
    RecordTimestamp(2);
    
    auto t2 = std::chrono::high_resolution_clock::now();
    // Comfort: [1, 64]
    RunModel(*m_comfortSession, {1, 64}, m_dummyComfortData);
    auto t3 = std::chrono::high_resolution_clock::now();

    RecordTimestamp(3);

    // Resolve timestamps
    m_commandAllocator->Reset();
    m_commandList->Reset(m_commandAllocator.Get(), nullptr);
    m_commandList->ResolveQueryData(m_queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0, 4, m_readbackBuffer.Get(), 0);
    m_commandList->Close();
    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(1, ppCommandLists);

    WaitForQueue();

    // Calculate GPU times
    uint64_t freq = 0;
    m_commandQueue->GetTimestampFrequency(&freq);
    double freqMs = (double)freq / 1000.0;
    
    uint64_t* pData = nullptr;
    m_readbackBuffer->Map(0, nullptr, reinterpret_cast<void**>(&pData));
    
    float flowGpuMs = static_cast<float>((pData[1] - pData[0]) / freqMs);
    float gazeGpuMs = static_cast<float>((pData[2] - pData[1]) / freqMs);
    float comfortGpuMs = static_cast<float>((pData[3] - pData[2]) / freqMs);
    float totalGpuMs = static_cast<float>((pData[3] - pData[0]) / freqMs);
    
    m_readbackBuffer->Unmap(0, nullptr);

    static int frameCount = 0;
    if (frameCount++ % 600 == 0) {
        float flowMs = std::chrono::duration<float, std::milli>(t1 - t0).count();
        float gazeMs = std::chrono::duration<float, std::milli>(t2 - t1).count();
        float comfortMs = std::chrono::duration<float, std::milli>(t3 - t2).count();
        float totalMs = std::chrono::duration<float, std::milli>(t3 - t0).count();
        
        LOG_INFO("AI Dispatch CPU -> Flow: %.3fms | Gaze: %.3fms | Comfort: %.3fms | Total: %.3fms", 
                 flowMs, gazeMs, comfortMs, totalMs);
        LOG_INFO("AI Dispatch GPU -> Flow: %.3fms | Gaze: %.3fms | Comfort: %.3fms | Total: %.3fms", 
                 flowGpuMs, gazeGpuMs, comfortGpuMs, totalGpuMs);
        LOG_INFO("Total Combined Sync Latency vs Budget (1.500ms): %.3fms", totalMs);
    }
}

} // namespace ai
} // namespace vrinject
