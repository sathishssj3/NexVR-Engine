---
source_file: "src/rendering/backends/dx12_renderer.h"
type: "code"
community: "Community 73"
location: "L30"
tags:
  - graphify/code
  - graphify/EXTRACTED
  - community/Community_73
---

# CreateIntermediateTexture

## Connections
- [[AllocateDescriptor]] - `calls` [INFERRED]
- [[CreateTexture_3]] - `calls` [INFERRED]
- [[DX12Renderer]] - `defines` [EXTRACTED]
- [[ExecuteTonemapToIntermediate]] - `calls` [INFERRED]
- [[TextureHandle]] - `references` [EXTRACTED]
- [[dx12_renderer.cpp]] - `contains` [EXTRACTED]

#graphify/code #graphify/EXTRACTED #community/Community_73