#include "matrix_classifier.h"
#include "../core/logger.h"
#include <cmath>

namespace vrinject {
namespace ai {

bool MatrixClassifier::Initialize(const std::wstring& modelPath) {
    // We are using the heuristic approach instead of ONNX.
    m_initialized = true;
    LOG_INFO("MatrixClassifier: Initialized Heuristic Math Scanner.");
    return true;
}

std::vector<std::pair<int, Prediction>> MatrixClassifier::ScanBuffer(const void* data, size_t size) {
    std::vector<std::pair<int, Prediction>> results;
    if (!m_initialized || size < 64 || data == nullptr) return results;

    const float* floatData = static_cast<const float*>(data);
    size_t count = size / sizeof(float);

    // Scan in strides, checking every possible 4x4 matrix alignment
    // (Games often align constant buffers to 256 bytes, but we scan fully just in case)
    for (size_t i = 0; i + 15 < count; i += 4) {
        // Perspective projection matrix heuristics
        float m00 = floatData[i + 0];  // xScale
        float m11 = floatData[i + 5];  // yScale
        float m22 = floatData[i + 10]; // zScale
        float m32 = floatData[i + 14]; // zTranslation
        float m23 = floatData[i + 11]; // +/- 1.0f (handedness)
        float m33 = floatData[i + 15]; // 0.0f

        // Check if w-component is 0 (classic perspective projection)
        if (std::abs(m33) > 0.0001f) continue;

        // Check handedness component (usually 1.0 or -1.0)
        if (std::abs(std::abs(m23) - 1.0f) > 0.001f) continue;

        // Check aspect ratio sanity (typically between 1.0 and 3.0)
        if (std::abs(m00) < 0.1f || std::abs(m11) < 0.1f) continue;
        float aspect = std::abs(m11 / m00);
        if (aspect < 0.5f || aspect > 4.0f) continue;

        // Passed heuristics - mark as highly confident Projection Matrix
        Prediction pred;
        pred.type = MatrixType::PerspectiveProjection;
        pred.confidence = 0.95f; 
        
        results.push_back({ static_cast<int>(i * sizeof(float)), pred });
        
        // Fast-forward since we found one
        i += 12; 
    }

    return results;
}

} // namespace ai
} // namespace vrinject
