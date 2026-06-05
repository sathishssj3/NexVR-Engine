#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include <cstdint>

namespace vrinject {

class ComfortGuard {
public:
    ComfortGuard();
    ~ComfortGuard();

    bool Initialize(ID3D11Device* device, const std::string& shaderDir);
    void Shutdown();

    // Dispatches the analysis compute shader and queues the copy to the staging buffer
    void AnalyzeDepth(ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* depthSRV, ID3D11Buffer* paramsCB, UINT width, UINT height);

    // Reads telemetry and updates dynamic parameters using temporal smoothing
    void UpdateParameters(ID3D11DeviceContext* ctx, float deltaTime, float& outConvergence, float& outDepthStrength);

    // Setters to sync with user baseline settings
    void SetBaseParams(float baseConvergence, float baseDepthStrength) {
        m_baseConvergence = baseConvergence;
        m_baseDepthStrength = baseDepthStrength;
    }

    // Getters for debug and telemetry
    const std::vector<uint32_t>& GetLastHistogram() const { return m_lastHistogram; }
    float GetCurrentConvergence() const { return m_currentConvergence; }
    float GetCurrentDepthStrength() const { return m_currentDepthStrength; }
    bool HasTelemetryData() const { return m_hasData; }

private:
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> m_analysisCS;

    // GPU histogram buffers (16 bins)
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_defaultBuffer;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> m_histogramUAV;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_stagingBuffer;

    // CPU telemetry
    std::vector<uint32_t> m_lastHistogram;
    bool m_hasData;

    // Base config
    float m_baseConvergence;
    float m_baseDepthStrength;

    // Smoothed state parameters
    float m_currentConvergence;
    float m_currentDepthStrength;
    float m_smoothingSpeed; // Exponential decay smoothing factor
};

} // namespace vrinject
