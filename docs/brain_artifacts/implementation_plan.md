# Stereoscopic 3D Implementation Plan (Non-Destructive)

## Goal
Implement stereoscopic 3D (depth reprojection) so the game stops looking like a flat TV screen and gains true 3D depth. 

**CRITICAL REQUIREMENT:** The user explicitly requested *not* to break the current working state (the game is finally displaying in VR!). Therefore, this plan will begin with a **100% non-destructive telemetry phase**. We will look, but we will not touch the rendering pipeline until we are absolutely sure what we are looking at.

## Current State
- The `VulkanDepthCandidateCollector` runs every frame to scan the game's render passes for Depth Buffers.
- The `DepthLockManager` analyzes these buffers to find the "Main" game depth buffer.
- **Problem:** Currently, neither of these systems output any logs. We have no idea if they are successfully finding the depth buffer for No Man's Sky/Sekiro, or if they are failing silently.

## Phase 1: Telemetry & Observation (Current Step)

Before we turn on 3D, we need to know if the engine is finding the depth buffer. I will add safe, throttled logging to the depth systems.

### Proposed Changes

#### [MODIFY] [vulkan_depth_candidate_collector.cpp](file:///c:/Users/sathi/.gemini/antigravity/scratch/vr-inject/src/core/vulkan_depth_candidate_collector.cpp)
- Add a throttle (e.g., log once every 60 frames) to print out how many depth candidates were found in the current frame.
- If candidates are found, log their resolution and format to verify they match the game's window size.

#### [MODIFY] [depth_lock_manager.cpp](file:///c:/Users/sathi/.gemini/antigravity/scratch/vr-inject/src/core/depth_lock_manager.cpp)
- Add a throttle to print when the state machine locks onto a depth buffer (`DepthLockState::LOCKED`).
- Log the confidence score and Reverse-Z detection results of the locked depth buffer.

## Phase 2: Stereoscopic Shader Activation (Next Step)
*Once we confirm via logs that the depth buffer is found:*
- Ensure `snapshot.depthBuffer` is populated in `Hooked_vkQueuePresentKHR`.
- Update the stereo compute shader (`vulkan_stereo_renderer.cpp`) to read the depth buffer and shift pixels horizontally based on their depth value (reprojection).
- Tune the IPD (Inter-Pupillary Distance) scaling factor so the 3D effect looks natural.

## Open Questions

> [!TIP]
> This phase is completely safe. We are only adding `LOG_INFO` statements. The game will continue to run exactly as it does now (flat screen in VR), but the log file will tell us if the 3D depth buffer is ready to be used.

Are you okay with me adding these telemetry logs and pushing a new DLL to test?
