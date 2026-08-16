#pragma once

#include <cstdint>
#include <vector>

namespace vrinject {
namespace ai {

// Generates a 2D segmentation mask to separate floating HUD/UI elements 
// from the 3D game world based on depth buffer heuristics.
class UIPlaneDetector {
public:
    UIPlaneDetector() = default;
    ~UIPlaneDetector() = default;

    // Analyzes the depth buffer and returns a binary mask (255 = UI, 0 = World)
    // The depthBuffer parameter is expected to be a readable linear array of float depths.
    std::vector<uint8_t> DetectUI(const float* depthBuffer, uint32_t width, uint32_t height);
    
    // Set depth threshold for classifying a pixel as UI (e.g., 0.0 or 1.0 depending on inverted depth)
    void SetThreshold(float threshold, float epsilon = 1e-4f);

private:
    float m_threshold = 1.0f; // Default assume 1.0 is UI
    float m_epsilon = 1e-4f;
};

} // namespace ai
} // namespace vrinject
