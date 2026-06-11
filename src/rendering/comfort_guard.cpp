#include "comfort_guard.h"
#include "../core/logger.h"
#include <fstream>
#include <vector>


namespace vrinject {

ComfortGuard::ComfortGuard() 
    : m_hasData(false)
    , m_baseConvergence(5.0f)
    , m_baseDepthStrength(1.0f)
    , m_currentConvergence(5.0f)
    , m_currentDepthStrength(1.0f)
    , m_smoothingSpeed(2.0f) 
{
    m_lastHistogram.resize(16, 0);
}

ComfortGuard::~ComfortGuard() {
    Shutdown();
}

bool ComfortGuard::Initialize(ID3D11Device* device, const std::string& shaderDir) {
    // 1. Load Compute Shader
    std::string shaderPath = shaderDir + "\\comfort_guard_analysis.cso";
    std::ifstream file(shaderPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        LOG_ERROR("ComfortGuard: Failed to open compute shader file: %s", shaderPath.c_str());
        return false;
    }
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> buffer(size);
    if (!file.read(buffer.data(), size)) {
        LOG_ERROR("ComfortGuard: Failed to read compute shader file: %s", shaderPath.c_str());
        return false;
    }
    HRESULT hr = device->CreateComputeShader(buffer.data(), size, nullptr, &m_analysisCS);
    if (FAILED(hr)) {
        LOG_ERROR("ComfortGuard: Failed to create compute shader (HR: 0x%X)", hr);
        return false;
    }

    // 2. Create Default GPU Buffer (with UAV bind flag for InterlockedAdd)
    D3D11_BUFFER_DESC bufDesc = {};
    bufDesc.ByteWidth = 16 * sizeof(uint32_t); // 16 bins
    bufDesc.Usage = D3D11_USAGE_DEFAULT;
    bufDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    bufDesc.CPUAccessFlags = 0;
    bufDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    bufDesc.StructureByteStride = sizeof(uint32_t);

    hr = device->CreateBuffer(&bufDesc, nullptr, &m_defaultBuffer);
    if (FAILED(hr)) {
        LOG_ERROR("ComfortGuard: Failed to create default GPU buffer (HR: 0x%X)", hr);
        return false;
    }

    // 3. Create UAV for the Default Buffer
    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN; // Must be UNKNOWN for structured buffers
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = 16;
    uavDesc.Buffer.Flags = 0;

    hr = device->CreateUnorderedAccessView(m_defaultBuffer.Get(), &uavDesc, &m_histogramUAV);
    if (FAILED(hr)) {
        LOG_ERROR("ComfortGuard: Failed to create UAV (HR: 0x%X)", hr);
        return false;
    }

    // 4. Create CPU Staging Buffer (for readback without pipeline stalls)
    D3D11_BUFFER_DESC stageDesc = {};
    stageDesc.ByteWidth = 16 * sizeof(uint32_t);
    stageDesc.Usage = D3D11_USAGE_STAGING;
    stageDesc.BindFlags = 0;
    stageDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stageDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    stageDesc.StructureByteStride = sizeof(uint32_t);

    hr = device->CreateBuffer(&stageDesc, nullptr, &m_stagingBuffer);
    if (FAILED(hr)) {
        LOG_ERROR("ComfortGuard: Failed to create staging buffer (HR: 0x%X)", hr);
        return false;
    }

    LOG_INFO("ComfortGuard dynamic depth analysis initialized successfully");
    return true;
}

void ComfortGuard::Shutdown() {
    m_analysisCS.Reset();
    m_defaultBuffer.Reset();
    m_histogramUAV.Reset();
    m_stagingBuffer.Reset();
    m_hasData = false;
}

void ComfortGuard::AnalyzeDepth(ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* depthSRV, ID3D11Buffer* paramsCB, UINT width, UINT height) {
    if (!m_analysisCS || !m_histogramUAV || !depthSRV) return;

    // 1. Clear the UAV on the GPU
    UINT clearValues[4] = { 0, 0, 0, 0 };
    ctx->ClearUnorderedAccessViewUint(m_histogramUAV.Get(), clearValues);

    // 2. Bind constant buffer (StereoParams)
    ID3D11Buffer* cbs[] = { paramsCB };
    ctx->CSSetConstantBuffers(0, 1, cbs);

    // 3. Bind resources
    ID3D11ShaderResourceView* srvs[] = { depthSRV };
    ctx->CSSetShaderResources(0, 1, srvs);

    ID3D11UnorderedAccessView* uavs[] = { m_histogramUAV.Get() };
    ctx->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

    // 4. Set Compute Shader
    ctx->CSSetShader(m_analysisCS.Get(), nullptr, 0);

    // 5. Dispatch
    // Thread group is [16, 16, 1]. Stride inside the shader is 8.
    // Subsampled grid width/height = ceil(Width / (8 * 16)) by ceil(Height / (8 * 16))
    UINT gridX = (width / 8 + 15) / 16;
    UINT gridY = (height / 8 + 15) / 16;
    ctx->Dispatch(gridX, gridY, 1);

    // 6. Clean up bindings to prevent resource hazards
    ID3D11ShaderResourceView* nullSRVs[1] = { nullptr };
    ctx->CSSetShaderResources(0, 1, nullSRVs);

    ID3D11UnorderedAccessView* nullUAVs[1] = { nullptr };
    ctx->CSSetUnorderedAccessViews(0, 1, nullUAVs, nullptr);

    // 7. Copy defaults to staging asynchronously (queues readback for the next frame)
    ctx->CopyResource(m_stagingBuffer.Get(), m_defaultBuffer.Get());
}

void ComfortGuard::UpdateParameters(ID3D11DeviceContext* ctx, float deltaTime, float& outConvergence, float& outDepthStrength) {
    if (!m_stagingBuffer) {
        outConvergence = m_baseConvergence;
        outDepthStrength = m_baseDepthStrength;
        return;
    }

    D3D11_MAPPED_SUBRESOURCE mapped;
    // Map staging buffer with DO_NOT_WAIT to avoid CPU stalls if the GPU isn't done writing
    HRESULT hr = ctx->Map(m_stagingBuffer.Get(), 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped);
    if (hr == DXGI_ERROR_WAS_STILL_DRAWING) {
        // GPU is busy. Return last known state to prevent stall.
        outConvergence = m_currentConvergence;
        outDepthStrength = m_currentDepthStrength;
        return;
    }
    if (SUCCEEDED(hr)) {
        const uint32_t* pHistogram = reinterpret_cast<const uint32_t*>(mapped.pData);
        m_lastHistogram.assign(pHistogram, pHistogram + 16);
        ctx->Unmap(m_stagingBuffer.Get(), 0);
        m_hasData = true;
    } else {
        // Fallback to last known smoothed state if mapping fails
        outConvergence = m_currentConvergence;
        outDepthStrength = m_currentDepthStrength;
        return;
    }

    // Accumulate total samples
    uint64_t totalSamples = 0;
    for (int i = 0; i < 16; ++i) {
        totalSamples += m_lastHistogram[i];
    }

    if (totalSamples == 0) {
        // Menu screens/loading screens with cleared depth buffers:
        // Decay smoothly back to baseline
        m_currentConvergence += (m_baseConvergence - m_currentConvergence) * m_smoothingSpeed * deltaTime;
        m_currentDepthStrength += (m_baseDepthStrength - m_currentDepthStrength) * m_smoothingSpeed * deltaTime;
        outConvergence = m_currentConvergence;
        outDepthStrength = m_currentDepthStrength;
        return;
    }

    // Heuristics calculation
    // Logarithmic Depth bins representing range [Near, Far].
    // Closer items are in lower bins (bins 0-4 represent < 3 meters).
    // Farrer vistas are in higher bins (bins 10-15 represent > 25 meters).
    
    double nearCount = 0;
    nearCount += m_lastHistogram[0] * 1.00;
    nearCount += m_lastHistogram[1] * 0.90;
    nearCount += m_lastHistogram[2] * 0.70;
    nearCount += m_lastHistogram[3] * 0.40;
    nearCount += m_lastHistogram[4] * 0.15;

    double farCount = 0;
    farCount += m_lastHistogram[10] * 0.10;
    farCount += m_lastHistogram[11] * 0.35;
    farCount += m_lastHistogram[12] * 0.55;
    farCount += m_lastHistogram[13] * 0.75;
    farCount += m_lastHistogram[14] * 0.90;
    farCount += m_lastHistogram[15] * 1.00;

    double nearOccupancy = nearCount / static_cast<double>(totalSamples);
    double farOccupancy = farCount / static_cast<double>(totalSamples);

    float targetConvergence = m_baseConvergence;
    float targetDepthStrength = m_baseDepthStrength;

    // Comfort control algorithms
    if (nearOccupancy > 0.05) {
        // High density of objects close to eyes!
        // 1. Move convergence closer (decreases overall near parallax)
        // 2. Dampen depth strength (flattens separation) to relieve convergence-accommodation conflict
        float nearScale = static_cast<float>(nearOccupancy);
        
        targetConvergence = m_baseConvergence - nearScale * 3.5f;
        if (targetConvergence < 1.5f) targetConvergence = 1.5f;

        targetDepthStrength = m_baseDepthStrength * (1.0f - nearScale * 0.7f);
        if (targetDepthStrength < 0.3f) targetDepthStrength = 0.3f;
    } 
    else if (farOccupancy > 0.35) {
        // Open vista!
        // 1. Push convergence back (makes background elements deeply layered)
        // 2. Expand depth strength slightly (up to 1.3x) for robust stereoscopic feel
        float farScale = static_cast<float>(farOccupancy - 0.35f) / 0.65f;
        
        targetConvergence = m_baseConvergence + farScale * 5.0f;
        if (targetConvergence > 10.0f) targetConvergence = 10.0f;

        targetDepthStrength = m_baseDepthStrength * (1.0f + farScale * 0.3f);
    }

    // Apply exponential decay temporal smoothing
    // Settle target variables over time to avoid structural popping
    m_currentConvergence += (targetConvergence - m_currentConvergence) * m_smoothingSpeed * deltaTime;
    m_currentDepthStrength += (targetDepthStrength - m_currentDepthStrength) * m_smoothingSpeed * deltaTime;

    outConvergence = m_currentConvergence;
    outDepthStrength = m_currentDepthStrength;
}

} // namespace vrinject
