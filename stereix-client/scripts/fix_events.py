import re

with open('src/rendering/backends/dx12_renderer.cpp', 'r') as f:
    content = f.read()

content = content.replace('m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);', 'm_syncFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);\n    m_vrFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);')

content = content.replace('if (m_fenceEvent) { CloseHandle(m_fenceEvent); m_fenceEvent = nullptr; }', 'if (m_syncFenceEvent) { CloseHandle(m_syncFenceEvent); m_syncFenceEvent = nullptr; }\n    if (m_vrFenceEvent) { CloseHandle(m_vrFenceEvent); m_vrFenceEvent = nullptr; }')

# In ExecuteTonemapToIntermediate
content = content.replace('m_syncFence->SetEventOnCompletion(fenceToWaitFor, m_fenceEvent);\n        WaitForSingleObject(m_fenceEvent, INFINITE);', 'm_syncFence->SetEventOnCompletion(fenceToWaitFor, m_syncFenceEvent);\n        WaitForSingleObject(m_syncFenceEvent, INFINITE);')

# In CopyToSwapchainVR
content = content.replace('m_vrFence->SetEventOnCompletion(m_vrFenceValue - 1, m_fenceEvent);\n        WaitForSingleObject(m_fenceEvent, INFINITE);', 'm_vrFence->SetEventOnCompletion(m_vrFenceValue - 1, m_vrFenceEvent);\n        WaitForSingleObject(m_vrFenceEvent, INFINITE);')

# In Flush
content = content.replace('m_syncFence->SetEventOnCompletion(currentFenceValue, m_fenceEvent);\n            WaitForSingleObject(m_fenceEvent, INFINITE);', 'm_syncFence->SetEventOnCompletion(currentFenceValue, m_syncFenceEvent);\n            WaitForSingleObject(m_syncFenceEvent, INFINITE);')
content = content.replace('m_vrFence->SetEventOnCompletion(m_vrFenceValue, m_fenceEvent);\n            WaitForSingleObject(m_fenceEvent, INFINITE);', 'm_vrFence->SetEventOnCompletion(m_vrFenceValue, m_vrFenceEvent);\n            WaitForSingleObject(m_vrFenceEvent, INFINITE);')

with open('src/rendering/backends/dx12_renderer.cpp', 'w') as f:
    f.write(content)

