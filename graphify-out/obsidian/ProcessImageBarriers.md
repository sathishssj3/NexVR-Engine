---
source_file: "src/core/vulkan_image_state_tracker.h"
type: "code"
community: "Community 130"
location: "L60"
tags:
  - graphify/code
  - graphify/EXTRACTED
  - community/Community_130
---

# ProcessImageBarriers

## Connections
- [[OnCmdPipelineBarrier]] - `calls` [INFERRED]
- [[OnCmdWaitEvents]] - `calls` [INFERRED]
- [[VkDevice_5]] - `references` [EXTRACTED]
- [[VkImageMemoryBarrier]] - `references` [EXTRACTED]
- [[VkPipelineStageFlags]] - `references` [EXTRACTED]
- [[VulkanImageStateTracker]] - `defines` [EXTRACTED]
- [[vulkan_image_state_tracker.cpp]] - `contains` [EXTRACTED]

#graphify/code #graphify/EXTRACTED #community/Community_130