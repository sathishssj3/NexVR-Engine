#pragma once
#include "core/stereo_types.h"
#include "heuristics/camera_snapshot.h"
#include "heuristics/render_frame_snapshot.h"

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
