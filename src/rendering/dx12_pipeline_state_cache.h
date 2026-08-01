#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include <mutex>

namespace vrinject {

class DX12PipelineStateCache {
public:
    DX12PipelineStateCache() = default;
    ~DX12PipelineStateCache();

    bool Initialize(ID3D12Device* device);
    void Shutdown();

    // Informs the cache that we are entering or leaving the safe initialization phase.
    // If a PSO or RootSignature is requested while not in init/resize, an assertion will fire.
    void SetSafeCreationPhase(bool isSafe);

    // Retrieves or creates the Root Signature for stereo rendering
    ID3D12RootSignature* GetStereoRootSignature(ID3D12Device* device);
    
    // Retrieves or creates the PSO for stereo reprojection
    ID3D12PipelineState* GetStereoPSO(ID3D12Device* device);

private:
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_stereoRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_stereoPSO;

    bool m_isSafeCreationPhase = false;
    mutable std::mutex m_mutex;

    bool CompileShader(const std::wstring& filename, const std::string& entrypoint, const std::string& target, std::vector<uint8_t>& outBytecode);
};

} // namespace vrinject
