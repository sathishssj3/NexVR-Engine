#pragma once

#include "ai_model_loader.h"

namespace NexVR {
namespace AI {

// Wraps depth_inpainter.onnx
class MidasInpainter : public AiModelLoader {
public:
    MidasInpainter(bool useDirectML)
        : AiModelLoader(L"depth_inpainter.onnx", useDirectML) {}

    // Runs inference to generate a depth map from an RGB input
    void GenerateDepthMap(const void* rgbFrameData, void* outDepthData) {
        // ... tensor binding and execution ...
    }
};

} // namespace AI
} // namespace NexVR
