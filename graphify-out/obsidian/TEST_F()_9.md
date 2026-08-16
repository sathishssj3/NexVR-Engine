---
source_file: "tests/test_vulkan_stress.cpp"
type: "code"
community: "Core Diagnostic Context Diagnosticc..."
location: "L163"
tags:
  - graphify/code
  - graphify/INFERRED
  - community/Core_Diagnostic_Context_Diagnosticc
---

# TEST_F()

## Connections
- [[GetState_2]] - `calls` [INFERRED]
- [[GetStateTracker]] - `calls` [INFERRED]
- [[InitializeVulkan]] - `calls` [INFERRED]
- [[Render10kFramesFullRenderCopyPath]] - `references` [EXTRACTED]
- [[RenderStereo_3]] - `calls` [INFERRED]
- [[SetOpenXRSwapchainImages]] - `calls` [INFERRED]
- [[Shutdown_21]] - `calls` [INFERRED]
- [[VulkanDispatchTable]] - `references` [EXTRACTED]
- [[VulkanLifecycleManager]] - `references` [EXTRACTED]
- [[VulkanStressTest]] - `references` [EXTRACTED]
- [[test_vulkan_stress.cpp]] - `contains` [EXTRACTED]

#graphify/code #graphify/INFERRED #community/Core_Diagnostic_Context_Diagnosticc