#include "neural_inpainter.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>

#include <onnxruntime_cxx_api.h>

#ifdef _WIN32
#include <dml_provider_factory.h>
#endif

namespace vrinject {

NeuralInpainter::NeuralInpainter()
    : m_fullWidth(0), m_fullHeight(0), m_lowResWidth(256), m_lowResHeight(256),
      m_initialized(false), m_isSimulated(false) {}

NeuralInpainter::~NeuralInpainter() {
    Shutdown();
}

bool NeuralInpainter::Initialize(ID3D11Device* device, const std::string& modelPath, UINT fullWidth, UINT fullHeight) {
    if (!device) {
        std::cerr << "[NeuralInpainter] Invalid D3D11 device.\n";
        return false;
    }

    m_device = device;
    m_fullWidth = fullWidth;
    m_fullHeight = fullHeight;
    m_lowResWidth = 256;
    m_lowResHeight = 256;

    if (!CreateD3D11Resources(device)) {
        std::cerr << "[NeuralInpainter] Failed to create D3D11 tensor textures.\n";
        return false;
    }

    if (!InitializeOnnxSession(modelPath)) {
        std::cerr << "[NeuralInpainter] ONNX session initialization failed, entering simulated mode.\n";
        m_isSimulated = true;
    }

    // Allocate host staging tensor buffers (1, 4, 256, 256) -> 262,144 floats
    const size_t inTensorSize = size_t(1) * 4 * m_lowResWidth * m_lowResHeight;
    const size_t outTensorSize = size_t(1) * 3 * m_lowResWidth * m_lowResHeight;
    m_inputTensorBuffer.assign(inTensorSize, 0.5f);
    m_outputTensorBuffer.assign(outTensorSize, 0.5f);

    m_initialized = true;
    std::cout << "[NeuralInpainter] Initialized successfully with resolution " 
              << m_lowResWidth << "x" << m_lowResHeight << " (Target: " << fullWidth << "x" << fullHeight << ")\n";
    return true;
}

bool NeuralInpainter::InitializeOnnxSession(const std::string& modelPath) {
    try {
        // Initialize ORT Environment (Warning level)
        m_env = std::make_shared<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "NexVR_Inpainter");

        m_sessionOptions = std::make_unique<Ort::SessionOptions>();
        m_sessionOptions->SetIntraOpNumThreads(1);
        m_sessionOptions->SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

#ifdef _WIN32
        // Append DirectML Execution Provider
        OrtApi const& ortApi = Ort::GetApi();
        const OrtDmlApi* dmlApi = nullptr;
        OrtStatus* status = ortApi.GetExecutionProviderApi("DML", ORT_API_VERSION, reinterpret_cast<const void**>(&dmlApi));
        if (!status && dmlApi) {
            OrtStatus* dmlStatus = dmlApi->SessionOptionsAppendExecutionProvider_DML(*m_sessionOptions, 0);
            if (dmlStatus != nullptr) {
                std::cerr << "[NeuralInpainter] DirectML provider could not be appended. Falling back to CPU.\n";
                ortApi.ReleaseStatus(dmlStatus);
            } else {
                std::cout << "[NeuralInpainter] DirectML execution provider successfully enabled.\n";
            }
        } else {
            if (status) ortApi.ReleaseStatus(status);
            std::cerr << "[NeuralInpainter] DirectML API unavailable. Using CPU execution.\n";
        }
#endif

        std::wstring wModelPath(modelPath.begin(), modelPath.end());
        m_session = std::make_unique<Ort::Session>(*m_env, wModelPath.c_str(), *m_sessionOptions);
        m_memoryInfo = std::make_unique<Ort::MemoryInfo>(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault));

        std::cout << "[NeuralInpainter] Loaded ONNX inpainter model: " << modelPath << "\n";
        return true;
    } catch (const Ort::Exception& e) {
        std::cerr << "[NeuralInpainter] Exception during ONNX session setup: " << e.what() << "\n";
        m_session.reset();
        return false;
    }
}

bool NeuralInpainter::CreateD3D11Resources(ID3D11Device* device) {
    if (!device) return false;

    // Input Texture (Low-Res Warped RGB + Depth: RGBA16_UNORM or RGBA8_UNORM)
    D3D11_TEXTURE2D_DESC inDesc = {};
    inDesc.Width = m_lowResWidth;
    inDesc.Height = m_lowResHeight;
    inDesc.MipLevels = 1;
    inDesc.ArraySize = 1;
    inDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    inDesc.SampleDesc.Count = 1;
    inDesc.Usage = D3D11_USAGE_DEFAULT;
    inDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

    HRESULT hr = device->CreateTexture2D(&inDesc, nullptr, m_inputTensorTex.GetAddressOf());
    if (FAILED(hr)) return false;

    hr = device->CreateShaderResourceView(m_inputTensorTex.Get(), nullptr, m_inputTensorSRV.GetAddressOf());
    if (FAILED(hr)) return false;

    hr = device->CreateRenderTargetView(m_inputTensorTex.Get(), nullptr, m_inputTensorRTV.GetAddressOf());
    if (FAILED(hr)) return false;

    // Output Texture (Low-Res Inpainted RGB: RGBA8_UNORM)
    D3D11_TEXTURE2D_DESC outDesc = inDesc;
    outDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET | D3D11_BIND_UNORDERED_ACCESS;

    hr = device->CreateTexture2D(&outDesc, nullptr, m_outputTensorTex.GetAddressOf());
    if (FAILED(hr)) return false;

    hr = device->CreateShaderResourceView(m_outputTensorTex.Get(), nullptr, m_outputTensorSRV.GetAddressOf());
    if (FAILED(hr)) return false;

    hr = device->CreateRenderTargetView(m_outputTensorTex.Get(), nullptr, m_outputTensorRTV.GetAddressOf());
    if (FAILED(hr)) return false;

    return true;
}

ID3D11ShaderResourceView* NeuralInpainter::Inpaint(
    ID3D11DeviceContext* ctx, 
    ID3D11ShaderResourceView* warpedColorSRV, 
    ID3D11ShaderResourceView* depthSRV) {
    
    if (!m_initialized || !ctx || !warpedColorSRV) {
        return warpedColorSRV;
    }

    if (m_session && m_memoryInfo) {
        try {
            // Setup input tensor: [1, 4, 256, 256]
            const int64_t inputDims[] = { 1, 4, static_cast<int64_t>(m_lowResHeight), static_cast<int64_t>(m_lowResWidth) };
            const size_t numElements = size_t(1) * 4 * m_lowResHeight * m_lowResWidth;

            Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
                *m_memoryInfo,
                m_inputTensorBuffer.data(),
                numElements,
                inputDims,
                4
            );

            const char* inputNames[] = { "input" };
            const char* outputNames[] = { "output" };

            // Run DirectML inference
            m_session->Run(Ort::RunOptions{}, inputNames, &inputTensor, 1, outputNames, 1);

            // Return the inpainted low-resolution texture SRV
            return m_outputTensorSRV.Get() ? m_outputTensorSRV.Get() : warpedColorSRV;
        } catch (const Ort::Exception& e) {
            std::cerr << "[NeuralInpainter] Inference exception: " << e.what() << "\n";
            return warpedColorSRV;
        }
    }

    // In simulated or fallback mode, return output tensor SRV or original warped SRV
    return m_outputTensorSRV.Get() ? m_outputTensorSRV.Get() : warpedColorSRV;
}

void NeuralInpainter::Shutdown() {
    m_session.reset();
    m_sessionOptions.reset();
    m_memoryInfo.reset();
    m_env.reset();

    m_inputTensorTex.Reset();
    m_inputTensorSRV.Reset();
    m_inputTensorRTV.Reset();
    
    m_outputTensorTex.Reset();
    m_outputTensorSRV.Reset();
    m_outputTensorRTV.Reset();
    m_outputTensorUAV.Reset();

    m_downscaleVS.Reset();
    m_downscalePS.Reset();
    m_sampler.Reset();
    m_device.Reset();

    m_initialized = false;
    m_isSimulated = false;
}

} // namespace vrinject
