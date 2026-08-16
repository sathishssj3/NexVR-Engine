---
source_file: "src/core/vulkan_image_state_tracker.h"
type: "code"
community: "Community 150"
location: "L21"
tags:
  - graphify/code
  - graphify/EXTRACTED
  - community/Community_150
---

# VulkanImageStateTracker

## Connections
- [[dot-VulkanImageStateTracker()]] - `method` [EXTRACTED]
- [[BuildSnapshot]] - `references` [EXTRACTED]
- [[DeviceImageKey]] - `references` [EXTRACTED]
- [[DeviceImageKeyHash]] - `references` [EXTRACTED]
- [[GetImageState]] - `defines` [EXTRACTED]
- [[Hooked_vkCmdPipelineBarrier()]] - `references` [EXTRACTED]
- [[Hooked_vkCmdWaitEvents()]] - `references` [EXTRACTED]
- [[ImageStateRecord]] - `references` [EXTRACTED]
- [[OnCmdPipelineBarrier]] - `defines` [EXTRACTED]
- [[OnCmdWaitEvents]] - `defines` [EXTRACTED]
- [[ProcessImageBarriers]] - `defines` [EXTRACTED]
- [[m_imageStates]] - `defines` [EXTRACTED]
- [[m_mutex_6]] - `defines` [EXTRACTED]
- [[shared_mutex_1]] - `references` [EXTRACTED]
- [[unordered_map_2]] - `references` [EXTRACTED]
- [[vulkan_image_state_tracker.h]] - `contains` [EXTRACTED]

#graphify/code #graphify/EXTRACTED #community/Community_150