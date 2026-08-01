#pragma once
#include "stereo_types.h"

namespace vrinject {

class StereoCameraGenerator {
public:
    static void Generate(const CameraSnapshot& camera, 
                         const StereoConstants& constants,
                         EyeView& outLeftEye,
                         EyeView& outRightEye);
};

} // namespace vrinject
