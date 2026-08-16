---
source_file: "src/rendering/backends/dx12_renderer.h"
type: "code"
community: "Community 73"
location: "L41"
tags:
  - graphify/code
  - graphify/INFERRED
  - community/Community_73
---

# ExecuteTonemapToIntermediate

## Connections
- [[dot-SwapIndices()]] - `calls` [INFERRED]
- [[AllocateDescriptor]] - `calls` [INFERRED]
- [[CreateIntermediateTexture]] - `calls` [INFERRED]
- [[DX12Renderer]] - `defines` [EXTRACTED]
- [[Flush]] - `calls` [INFERRED]
- [[TextureHandle]] - `references` [EXTRACTED]
- [[WaitForFence]] - `calls` [INFERRED]
- [[dx12_renderer.cpp]] - `contains` [EXTRACTED]

#graphify/code #graphify/INFERRED #community/Community_73