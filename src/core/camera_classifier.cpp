#include "camera_classifier.h"
#include "logger.h"
#include <atomic>
#include <cmath>
#include <cstring>

namespace vrinject {

static inline float M(const Matrix4x4& mat, int r, int c, bool rowMajor) {
    if (rowMajor) return mat.m[r][c];
    return mat.m[c][r];
}

bool CameraClassifier::IsPerspectiveProjection(const Matrix4x4& mat, bool rowMajor, bool& outReverseZ) {
    // Check characteristic perspective projection traits
    // m33 == 0 (w component getting mapped from z) or m33 == -1 (OpenGL style, but mostly DX here)
    // m23 != 0 (w getting depth)
    // DX perspective: m[3][3] = 0, m[2][3] = 1 or -1
    
    float m33 = M(mat, 3, 3, rowMajor);
    float m23 = M(mat, 2, 3, rowMajor);

    if (std::abs(m33) > 0.001f) {
        return false; // Typically 0 in projection
    }

    if (std::abs(m23 - 1.0f) > 0.01f && std::abs(m23 + 1.0f) > 0.01f) {
        return false;
    }

    // Check Z-mapping (m22 and m32)
    float m22 = M(mat, 2, 2, rowMajor);
    // float m32 = M(mat, 3, 2, rowMajor); // Unused for now, but part of depth bias
    
    // Standard Z goes to 1, Reverse Z goes to 0
    // In DX, standard perspective has m22 around 1 (e.g. zf / (zf-zn))
    // Reverse Z has m22 around 0 (e.g. zn / (zn-zf) )
    if (std::abs(m22) < 0.1f) {
        outReverseZ = true;
    } else {
        outReverseZ = false;
    }

    return true;
}

bool CameraClassifier::IsOrthographicProjection(const Matrix4x4& mat, bool rowMajor) {
    // Ortho: m33 = 1, m23 = 0, m03 = 0, m13 = 0
    float m33 = M(mat, 3, 3, rowMajor);
    float m23 = M(mat, 2, 3, rowMajor);
    float m03 = M(mat, 0, 3, rowMajor);
    float m13 = M(mat, 1, 3, rowMajor);

    if (std::abs(m33 - 1.0f) > 0.001f) return false;
    if (std::abs(m23) > 0.001f) return false;
    if (std::abs(m03) > 0.001f) return false;
    if (std::abs(m13) > 0.001f) return false;

    // Check scale factors (m00, m11, m22)
    float m00 = M(mat, 0, 0, rowMajor);
    float m11 = M(mat, 1, 1, rowMajor);
    
    if (std::abs(m00) < 0.0001f || std::abs(m11) < 0.0001f) return false;

    return true;
}

bool CameraClassifier::ClassifyMatrix(const Matrix4x4& m, bool& outRowMajor, bool& outReverseZ) {
    // Try both row-major and col-major
    bool revZ = false;
    if (IsPerspectiveProjection(m, true, revZ)) {
        outRowMajor = true;
        outReverseZ = revZ;
        return true;
    }
    
    if (IsPerspectiveProjection(m, false, revZ)) {
        outRowMajor = false;
        outReverseZ = revZ;
        return true;
    }

    return false;
}

bool CameraClassifier::TryCreateCandidate(uint64_t id, void* cbAddress, const Matrix4x4& m, CameraCandidate& outCandidate) {
    bool rowMajor = true;
    bool reverseZ = false;

    if (!ClassifyMatrix(m, rowMajor, reverseZ)) {
        return false;
    }

    std::memset(&outCandidate, 0, sizeof(CameraCandidate));
    outCandidate.id = id;
    outCandidate.constantBuffer = cbAddress;
    outCandidate.projection = m;
    
    // We assume the matrix we intercepted could be ViewProjection or just Projection.
    // Differentiating them comes later in validation, but for now we'll assume it's ViewProjection if it has translation
    // Wait, typical projection matrices have m30 = 0, m31 = 0. If they are non-zero, it's likely a ViewProjection.
    float m30 = M(m, 3, 0, rowMajor);
    float m31 = M(m, 3, 1, rowMajor);

    if (std::abs(m30) > 0.001f || std::abs(m31) > 0.001f) {
        outCandidate.viewProjection = m;
        // View and projection split is complex without more context, but we store VP.
    } else {
        outCandidate.projection = m;
    }

    outCandidate.rowMajor = rowMajor;
    outCandidate.reversedZ = reverseZ;
    outCandidate.valid = true;
    outCandidate.confidence = 0.5f; // Baseline confidence for passing projection test

    // Handedness probe. sign(m[2][3]) is the one bit that says whether the game
    // uses +Z forward (left-handed, DX default) or -Z forward (right-handed,
    // GL/OpenXR default). IsPerspectiveProjection accepts either at :25 and
    // stores neither, and no field on CameraCandidate/CameraSnapshot records it -
    // so 6DOF head-pose composition currently has no way to know whether the
    // OpenXR pose needs a Z-flip before it is combined with the game view.
    // Getting that wrong inverts yaw and roll, which is acutely nauseating and
    // invisible in every log, so it must be measured in a real game rather than
    // guessed. Logged from a live title, one line settles it.
    //
    // Rate-limited: this runs on the render thread for every accepted candidate.
    
    const float m23 = M(m, 2, 3, rowMajor);
    outCandidate.isLeftHanded = (m23 > 0.0f);

    static std::atomic<int> s_probesLogged{0};
    if (s_probesLogged.fetch_add(1, std::memory_order_relaxed) < 16) {
        const bool isVP = (std::abs(m30) > 0.001f || std::abs(m31) > 0.001f);
        LOG_INFO("CameraProbe: id=%llu m23=%+.4f handed=%s rowMajor=%d reverseZ=%d kind=%s m00=%+.4f",
                 (unsigned long long)id,
                 m23,
                 (m23 > 0.0f) ? "LEFT(+Z fwd)" : "RIGHT(-Z fwd)",
                 rowMajor ? 1 : 0,
                 reverseZ ? 1 : 0,
                 isVP ? "ViewProjection" : "Projection",
                 M(m, 0, 0, rowMajor));
    }

    return true;
}

} // namespace vrinject
