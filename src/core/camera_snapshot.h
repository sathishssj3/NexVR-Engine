#pragma once
#include "math_types.h"
#include "graphics_types.h"
#include <cstdint>

namespace vrinject {

struct CameraSnapshot
{
    uint64_t frame;

    Matrix4x4 view;
    Matrix4x4 projection;
    Matrix4x4 vp;

    Vector3 position;
    Quaternion rotation;

    float confidence;
    bool reversedZ;
    bool isLeftHanded;
    bool valid;
    
    GraphicsResourceIdentity resourceIdentity;

    bool IsValid() const {
        return valid && confidence > 0.5f && resourceIdentity.IsValid();
    }
};

} // namespace vrinject
