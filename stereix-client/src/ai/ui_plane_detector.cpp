#include "ai/ui_plane_detector.h"
#include <cmath>

namespace vrinject {
namespace ai {

void UIPlaneDetector::SetThreshold(float threshold, float epsilon) {
    m_threshold = threshold;
    m_epsilon = epsilon;
}

std::vector<uint8_t> UIPlaneDetector::DetectUI(const float* depthBuffer, uint32_t width, uint32_t height) {
    std::vector<uint8_t> mask(width * height, 0);

    if (!depthBuffer) {
        return mask;
    }

    for (uint32_t i = 0; i < width * height; ++i) {
        float depth = depthBuffer[i];
        
        // Simple heuristic: if depth is extremely close to the threshold (usually 1.0 for far plane, or 0.0 for near plane),
        // we assume it is UI that didn't write depth properly or was rendered on a quad at the camera bounds.
        if (std::abs(depth - m_threshold) <= m_epsilon) {
            mask[i] = 255;
        } else {
            mask[i] = 0;
        }
    }

    // Advanced: here we would normally do region growing or Connected Component Analysis 
    // to filter out noise, but simple thresholding works as a baseline.
    
    return mask;
}

} // namespace ai
} // namespace vrinject
