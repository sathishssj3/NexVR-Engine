#pragma once
#include "stereo_types.h"

namespace vrinject {

class StereoFrameBuilder {
public:
    static StereoFrameContext Build(uint64_t frameId,
                                    const CameraSnapshot& camera,
                                    const DepthSnapshot& depth,
                                    const EyeView& leftEye,
                                    const EyeView& rightEye,
                                    const StereoConstants& constants,
                                    uint32_t width,
                                    uint32_t height);
};

} // namespace vrinject
