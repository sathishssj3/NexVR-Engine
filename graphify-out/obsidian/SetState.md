---
source_file: "src/core/vulkan_lifecycle_manager.h"
type: "code"
community: "Community 12"
location: "L36"
tags:
  - graphify/code
  - graphify/INFERRED
  - community/Community_12
---

# SetState

## Connections
- [[OnDeviceCreated]] - `calls` [INFERRED]
- [[OnDeviceDestroyed]] - `calls` [INFERRED]
- [[OnInstanceCreated]] - `calls` [INFERRED]
- [[OnInstanceDestroyed]] - `calls` [INFERRED]
- [[OnSwapchainCreated]] - `calls` [INFERRED]
- [[OnSwapchainDestroyed]] - `calls` [INFERRED]
- [[RenderState]] - `references` [EXTRACTED]
- [[ReportDeviceLost]] - `calls` [INFERRED]
- [[VulkanLifecycleManager]] - `defines` [EXTRACTED]
- [[vulkan_lifecycle_manager.cpp]] - `contains` [EXTRACTED]

#graphify/code #graphify/INFERRED #community/Community_12