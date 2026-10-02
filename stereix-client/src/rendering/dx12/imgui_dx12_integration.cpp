#include "rendering/dx12/imgui_dx12_integration.h"
#include "core/logger.h"
#include "core/overlay_manager.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"

namespace vrinject {

bool ImGuiDX12Integration::Initialize(ID3D12Device* device, DXGI_FORMAT rtvFormat) {
    if (m_initialized) return true;
    if (!device) return false;
    if (!ImGui::GetCurrentContext()) return false;

    // Create SRV Descriptor Heap for ImGui (needs 1 descriptor for font texture)
    D3D12_DESCRIPTOR_HEAP_DESC srvDesc = {};
    srvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvDesc.NumDescriptors = 1;
    srvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(device->CreateDescriptorHeap(&srvDesc, IID_PPV_ARGS(&m_srvDescHeap)))) {
        LOG_ERROR("ImGuiDX12: Failed to create SRV descriptor heap");
        return false;
    }

    // Create RTV Descriptor Heap for ImGui (2 descriptors for left and right eyes)
    D3D12_DESCRIPTOR_HEAP_DESC rtvDesc = {};
    rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvDesc.NumDescriptors = 2;
    rtvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    if (FAILED(device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&m_rtvDescHeap)))) {
        LOG_ERROR("ImGuiDX12: Failed to create RTV descriptor heap");
        return false;
    }
    m_rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    if (!ImGui_ImplDX12_Init(device, 3,
        rtvFormat,
        m_srvDescHeap.Get(),
        m_srvDescHeap->GetCPUDescriptorHandleForHeapStart(),
        m_srvDescHeap->GetGPUDescriptorHandleForHeapStart())) {
        LOG_ERROR("ImGuiDX12: ImGui_ImplDX12_Init failed!");
        return false;
    }

    m_initialized = true;
    LOG_INFO("ImGuiDX12: Initialized successfully with unified command list integration.");
    return true;
}

void ImGuiDX12Integration::Render(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, ID3D12Resource* leftDest, ID3D12Resource* rightDest) {
    if (!m_initialized || !device || !cmdList || !leftDest) return;

    // Set Descriptor Heaps
    ID3D12DescriptorHeap* heaps[] = { m_srvDescHeap.Get() };
    cmdList->SetDescriptorHeaps(1, heaps);

    // Generate ImGui DrawData ONCE per frame
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    OverlayManager::GetInstance().Render();
    ImDrawData* drawData = ImGui::GetDrawData();

    if (drawData && drawData->TotalVtxCount > 0) {
        // Draw to Left Eye
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvDescHeap->GetCPUDescriptorHandleForHeapStart();
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = leftDest->GetDesc().Format;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        device->CreateRenderTargetView(leftDest, &rtvDesc, rtvHandle);

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = leftDest;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        cmdList->ResourceBarrier(1, &barrier);

        cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
        ImGui_ImplDX12_RenderDrawData(drawData, cmdList);

        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        cmdList->ResourceBarrier(1, &barrier);

        // Copy menu overlay to Right Eye so both eyes see the exact same HUD
        if (rightDest && rightDest != leftDest) {
            D3D12_RESOURCE_BARRIER copyBarriers[2] = {};
            copyBarriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            copyBarriers[0].Transition.pResource = leftDest;
            copyBarriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            copyBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            copyBarriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

            copyBarriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            copyBarriers[1].Transition.pResource = rightDest;
            copyBarriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            copyBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            copyBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

            cmdList->ResourceBarrier(2, copyBarriers);
            cmdList->CopyResource(rightDest, leftDest);

            copyBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
            copyBarriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            copyBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            copyBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            cmdList->ResourceBarrier(2, copyBarriers);
        }
    }
}

void ImGuiDX12Integration::Shutdown() {
    if (m_initialized) {
        ImGui_ImplDX12_Shutdown();
        m_srvDescHeap.Reset();
        m_rtvDescHeap.Reset();
        m_initialized = false;
    }
}

} // namespace vrinject
