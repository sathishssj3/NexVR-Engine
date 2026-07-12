#include "imgui_dx12_integration.h"
#include "../core/logger.h"
#include "../core/overlay_manager.h"
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"

namespace vrinject {

bool ImGuiDX12Integration::Initialize(ID3D12Device* device, DXGI_FORMAT rtvFormat) {
    if (m_initialized) return true;

    // Create SRV Descriptor Heap for ImGui (needs 1 descriptor for font texture)
    D3D12_DESCRIPTOR_HEAP_DESC srvDesc = {};
    srvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvDesc.NumDescriptors = 1;
    srvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(device->CreateDescriptorHeap(&srvDesc, IID_PPV_ARGS(&m_srvDescHeap)))) {
        LOG_ERROR("ImGuiDX12: Failed to create SRV descriptor heap");
        return false;
    }

    // Create RTV Descriptor Heap for ImGui to render to the backbuffer
    D3D12_DESCRIPTOR_HEAP_DESC rtvDesc = {};
    rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvDesc.NumDescriptors = 1;
    rtvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    if (FAILED(device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&m_rtvDescHeap)))) {
        LOG_ERROR("ImGuiDX12: Failed to create RTV descriptor heap");
        return false;
    }
    m_rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_cmdAlloc)))) {
        LOG_ERROR("ImGuiDX12: Failed to create CommandAllocator");
        return false;
    }

    if (FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_cmdAlloc.Get(), nullptr, IID_PPV_ARGS(&m_cmdList)))) {
        LOG_ERROR("ImGuiDX12: Failed to create CommandList");
        return false;
    }
    m_cmdList->Close();

    if (!ImGui_ImplDX12_Init(device, 2,
        rtvFormat,
        m_srvDescHeap.Get(),
        m_srvDescHeap->GetCPUDescriptorHandleForHeapStart(),
        m_srvDescHeap->GetGPUDescriptorHandleForHeapStart())) {
        LOG_ERROR("ImGuiDX12: ImGui_ImplDX12_Init failed!");
        return false;
    }

    m_initialized = true;
    LOG_INFO("ImGuiDX12: Initialized successfully");
    return true;
}

void ImGuiDX12Integration::Render(ID3D12Device* device, ID3D12CommandQueue* queue, ID3D12Resource* backBuffer) {
    if (!m_initialized) return;

    // Reset command allocator and list
    m_cmdAlloc->Reset();
    m_cmdList->Reset(m_cmdAlloc.Get(), nullptr);

    // Create RTV for the current backbuffer
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvDescHeap->GetCPUDescriptorHandleForHeapStart();
    device->CreateRenderTargetView(backBuffer, nullptr, rtvHandle);

    // Transition backbuffer to RENDER_TARGET if needed (we assume it already is, but safety first)
    // Actually, in the DX12 hook before Present, it is usually in RENDER_TARGET or PRESENT state.
    // We will just bind it and draw.
    
    // Set Descriptor Heaps
    ID3D12DescriptorHeap* heaps[] = { m_srvDescHeap.Get() };
    m_cmdList->SetDescriptorHeaps(1, heaps);

    // Set Render Target
    m_cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    // Render ImGui
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    
    OverlayManager::GetInstance().Render();
    
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_cmdList.Get());

    m_cmdList->Close();

    // Execute Command List
    ID3D12CommandList* commandLists[] = { m_cmdList.Get() };
    queue->ExecuteCommandLists(1, commandLists);
}

void ImGuiDX12Integration::Shutdown() {
    if (m_initialized) {
        ImGui_ImplDX12_Shutdown();
        m_srvDescHeap.Reset();
        m_rtvDescHeap.Reset();
        m_cmdAlloc.Reset();
        m_cmdList.Reset();
        m_initialized = false;
    }
}

} // namespace vrinject
