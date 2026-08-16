#pragma once
#include "stereo_types.h"
#include "camera_snapshot.h"
#include "render_frame_snapshot.h"

namespace vrinject {

class StereoCameraGenerator {
public:
    static void Generate(const RenderFrameSnapshot& renderSnapshot,
                         const CameraSnapshot& camera, 
                         const StereoConstants& constants,
                         EyeView& outLeftEye,
                         EyeView& outRightEye);
};

} // namespace vrinject
