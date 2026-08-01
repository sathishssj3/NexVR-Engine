#pragma once
#include <cstdint>

namespace vrinject {

struct Vector3 {
    float x, y, z;
};

struct Quaternion {
    float x, y, z, w;
};

struct Matrix4x4 {
    union {
        float m[4][4];
        float m16[16];
    };
};

} // namespace vrinject
