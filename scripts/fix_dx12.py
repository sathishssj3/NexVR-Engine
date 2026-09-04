import re

with open('src/rendering/backends/dx12_renderer.cpp', 'r') as f:
    content = f.read()

# 1. Implement Flush()
flush_impl = '''void DX12Renderer::DestroyTexture(TextureHandle& handle) {
    if (handle.nativePtr) {
        static_cast<ID3D12Resource*>(handle.nativePtr)->Release();
        handle.nativePtr = nullptr;
    }
    // Descriptors are ring-buffered in this prototype, no explicit free
}

void DX12Renderer::Flush() {
    if (!m_device) return;
    if (m_gameCommandQueue && m_syncFence) {
        uint64_t currentFenceValue = m_fenceValues[0] > m_fenceValues[1] ? m_fenceValues[0] : m_fenceValues[1];
        currentFenceValue++;
        m_gameCommandQueue->Signal(m_syncFence, currentFenceValue);
        if (m_syncFence->GetCompletedValue() < currentFenceValue) {
            m_syncFence->SetEventOnCompletion(currentFenceValue, m_fenceEvent);
            WaitForSingleObject(m_fenceEvent, INFINITE);
        }
    }
    if (m_vrCommandQueue && m_vrFence) {
        m_vrFenceValue++;
        m_vrCommandQueue->Signal(m_vrFence, m_vrFenceValue);
        if (m_vrFence->GetCompletedValue() < m_vrFenceValue) {
            m_vrFence->SetEventOnCompletion(m_vrFenceValue, m_fenceEvent);
            WaitForSingleObject(m_fenceEvent, INFINITE);
        }
    }
}'''

content = re.sub(r'void DX12Renderer::DestroyTexture\(TextureHandle& handle\).*?\}', flush_impl, content, flags=re.DOTALL)

# 2. ExecuteTonemapToIntermediate Mutex
exec_old = '''    int writeIdx = m_writeIndex.load();
    
    uint32_t targetW = (m_vrWidth > 0) ? m_vrWidth : source.width;'''

exec_new = '''    int writeIdx;
    uint64_t vrFenceToWaitFor;
    {
        std::lock_guard<std::mutex> lock(m_indexMutex);
        writeIdx = m_writeIndex.load();
        vrFenceToWaitFor = m_vrReadFenceValues[writeIdx];
    }
    
    uint32_t targetW = (m_vrWidth > 0) ? m_vrWidth : source.width;'''

content = content.replace(exec_old, exec_new)

# 3. Add Flush before DestroyTexture
content = content.replace('if (m_intermediateTextures[writeIdx].nativePtr) DestroyTexture(m_intermediateTextures[writeIdx]);', 'Flush();\n        if (m_intermediateTextures[writeIdx].nativePtr) DestroyTexture(m_intermediateTextures[writeIdx]);')
content = content.replace('if (m_rawIntermediateTextures[writeIdx].nativePtr) DestroyTexture(m_rawIntermediateTextures[writeIdx]);', 'Flush();\n            if (m_rawIntermediateTextures[writeIdx].nativePtr) DestroyTexture(m_rawIntermediateTextures[writeIdx]);')

# 4. Remove redundant vrFenceToWaitFor
redundant_fence_old = '''        // Wait for the VR queue to finish reading from this texture before we overwrite it
        uint64_t vrFenceToWaitFor = m_vrReadFenceValues[writeIdx];
        if (vrFenceToWaitFor > 0) {'''

redundant_fence_new = '''        // Wait for the VR queue to finish reading from this texture before we overwrite it
        if (vrFenceToWaitFor > 0) {'''

content = content.replace(redundant_fence_old, redundant_fence_new)

# 5. SwapIndices Mutex
swap_old = '''void DX12Renderer::SwapIndices() {
    int currentWrite = m_writeIndex.load();'''

swap_new = '''void DX12Renderer::SwapIndices() {
    std::lock_guard<std::mutex> lock(m_indexMutex);
    int currentWrite = m_writeIndex.load();'''

content = content.replace(swap_old, swap_new)

# 6. CopyToSwapchainVR Mutex
copy_old = '''void DX12Renderer::CopyToSwapchainVR(void* swapchainTexture) {
    int readIdx = m_readIndex.load();
    if (!m_device || !m_intermediateTextures[readIdx].nativePtr || !swapchainTexture) return;'''

copy_new = '''void DX12Renderer::CopyToSwapchainVR(void* swapchainTexture) {
    int readIdx;
    {
        std::lock_guard<std::mutex> lock(m_indexMutex);
        readIdx = m_readIndex.load();
        m_vrFenceValue++;
        m_vrReadFenceValues[readIdx] = m_vrFenceValue;
    }
    if (!m_device || !m_intermediateTextures[readIdx].nativePtr || !swapchainTexture) return;'''

content = content.replace(copy_old, copy_new)

with open('src/rendering/backends/dx12_renderer.cpp', 'w') as f:
    f.write(content)
