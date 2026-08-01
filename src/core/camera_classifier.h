#pragma once
#include "camera_candidate.h"
#include <vector>

namespace vrinject {

class CameraClassifier {
public:
    // Pure function: evaluates a matrix and extracts metadata if it looks like a camera projection or view-projection matrix
    static bool ClassifyMatrix(const Matrix4x4& m, bool& outRowMajor, bool& outReversedTest);
    
    // Validates if it's a projection matrix
    static bool IsPerspectiveProjection(const Matrix4x4& m, bool rowMajor, bool& outReverseZ);

    // Validates if it's an orthographic projection
    static bool IsOrthographicProjection(const Matrix4x4& m, bool rowMajor);

    // Creates a candidate if it passes basic validation
    static bool TryCreateCandidate(uint64_t id, void* cbAddress, const Matrix4x4& m, CameraCandidate& outCandidate);
};

} // namespace vrinject
