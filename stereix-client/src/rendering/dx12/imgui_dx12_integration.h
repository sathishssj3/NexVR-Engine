#pragma once
#include <d3d12.h>
#include <wrl/client.h>

namespace vrinject {

class ImGuiDX12Integration {
public:
    static ImGuiDX12Integration& GetInstance() {
        static ImGuiDX12Integration instance;
        return instance;
    }

    bool Initialize(ID3D12Device* device, DXGI_FORMAT rtvFormat);
    void Render(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, ID3D12Resource* leftDest, ID3D12Resource* rightDest = nullptr);
    void Shutdown();

private:
    ImGuiDX12Integration() = default;
    ~ImGuiDX12Integration() = default;

    bool m_initialized = false;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvDescHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvDescHeap;
    UINT m_rtvDescriptorSize = 0;
};

} // namespace vrinject
