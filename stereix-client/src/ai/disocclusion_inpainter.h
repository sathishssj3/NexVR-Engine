#pragma once

#include "ai/ai_model_loader.h"
#include <vector>
#include <mutex>

namespace vrinject {
namespace ai {

class DisocclusionInpainter : public NexVR::AI::AiModelLoader {
public:
    static DisocclusionInpainter& GetInstance(bool useDirectML = true, int deviceId = 0);

    DisocclusionInpainter(bool useDirectML = true, int deviceId = 0);
    ~DisocclusionInpainter() override = default;

    // Runs neural inpainting on the provided RGB image and mask buffer
    // rgbInput: float array [3 * width * height] normalized 0.0 to 1.0 (RGB planar)
    // maskInput: float array [1 * width * height] (1.0 = hole to fill, 0.0 = valid pixel)
    // rgbOutput: float array [3 * width * height] receives the inpainted RGB image
    bool Inpaint(
        const float* rgbInput,
        const float* maskInput,
        float* rgbOutput,
        int width,
        int height
    );

private:
    std::mutex m_mutex;
};

} // namespace ai
} // namespace vrinject
