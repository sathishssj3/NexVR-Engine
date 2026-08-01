#include "depth_validator.h"

namespace vrinject {

void DepthValidator::Validate(DepthCandidate& candidate, uint32_t swapchainWidth, uint32_t swapchainHeight) {
    if (!candidate.valid) return;

    // 1. Resolution checks
    // Reject if width or height is less than 50% of swapchain (likely shadow cascade or minimap)
    if (swapchainWidth > 0 && swapchainHeight > 0) {
        if (candidate.identity.width < swapchainWidth / 2 || 
            candidate.identity.height < swapchainHeight / 2) {
            candidate.valid = false;
            return;
        }
        
        // Resolution match score (for Ranking Engine)
        if (candidate.identity.width == swapchainWidth && candidate.identity.height == swapchainHeight) {
            candidate.resolutionScore = 1.0f;
        } else {
            candidate.resolutionScore = 0.5f;
        }
    }

    // 2. Texture type checks
    // Cubemaps usually have arraySize == 6 and are used for reflections or point light shadows
    if (candidate.identity.arraySize == 6) {
        candidate.valid = false;
        return;
    }
    
    // Array textures used for shadow cascades
    if (candidate.identity.arraySize > 1 && candidate.identity.arraySize != 2) { // 2 might be stereo
        candidate.valid = false;
        return;
    }

    // 3. Usage checks
    // Reject if it was never cleared AND only bound a few times (transient or UI)
    if (candidate.clearCount == 0 && candidate.bindCount <= 1) {
        // We require at least some consistent usage or clearing for a main depth buffer
        // Note: deferred renderers might not clear depth if they overwrite entirely, 
        // but they will bind it many times for G-buffer and lighting passes.
        candidate.valid = false;
        return;
    }
}

} // namespace vrinject
