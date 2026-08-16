---
source_file: "src/core/vulkan_image_state_tracker.h"
type: "code"
community: "Community 130"
location: "L41"
tags:
  - graphify/code
  - graphify/EXTRACTED
  - community/Community_130
---

# OnCmdWaitEvents

## Connections
- [[ProcessImageBarriers]] - `calls` [INFERRED]
- [[VkBufferMemoryBarrier]] - `references` [EXTRACTED]
- [[VkCommandBuffer_1]] - `references` [EXTRACTED]
- [[VkDevice_5]] - `references` [EXTRACTED]
- [[VkEvent]] - `references` [EXTRACTED]
- [[VkImageMemoryBarrier]] - `references` [EXTRACTED]
- [[VkMemoryBarrier]] - `references` [EXTRACTED]
- [[VkPipelineStageFlags]] - `references` [EXTRACTED]
- [[VulkanImageStateTracker]] - `defines` [EXTRACTED]
- [[vulkan_image_state_tracker.cpp]] - `contains` [EXTRACTED]

#graphify/code #graphify/EXTRACTED #community/Community_130