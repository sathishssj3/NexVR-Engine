---
source_file: "src/rendering/backends/dx12_renderer.h"
type: "code"
community: "Community 73"
location: "L29"
tags:
  - graphify/code
  - graphify/EXTRACTED
  - community/Community_73
---

# WaitForFence

## Connections
- [[CopyToSwapchainVR]] - `calls` [INFERRED]
- [[DX12Renderer]] - `defines` [EXTRACTED]
- [[ExecuteTonemapToIntermediate]] - `calls` [INFERRED]
- [[Flush]] - `calls` [INFERRED]
- [[HANDLE_4]] - `references` [EXTRACTED]
- [[ID3D12Fence_3]] - `references` [EXTRACTED]
- [[SkipVRFrame]] - `calls` [INFERRED]
- [[dx12_renderer.cpp]] - `contains` [EXTRACTED]

#graphify/code #graphify/EXTRACTED #community/Community_73