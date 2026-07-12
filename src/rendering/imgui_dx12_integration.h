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
    void Render(ID3D12Device* device, ID3D12CommandQueue* queue, ID3D12Resource* backBuffer);
    void Shutdown();

private:
    ImGuiDX12Integration() = default;
    ~ImGuiDX12Integration() = default;

    bool m_initialized = false;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvDescHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvDescHeap;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_cmdAlloc;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_cmdList;
    UINT m_rtvDescriptorSize = 0;
};

} // namespace vrinject
