#pragma once
#include "core/math_types.h"
#include <cstdint>

namespace vrinject {

struct CameraCandidate
{
    uint64_t id;
    void* constantBuffer;

    Matrix4x4 view;
    Matrix4x4 projection;
    Matrix4x4 viewProjection;

    float confidence;
    float temporalScore;
    uint64_t firstSeenFrame;
    uint64_t lastSeenFrame;
    uint32_t updateCount;

    bool rowMajor;
    bool reversedZ;
    bool isLeftHanded;
    bool valid;
};

} // namespace vrinject
