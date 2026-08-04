#pragma once

#include "ai_model_loader.h"

namespace NexVR {
namespace AI {

// Wraps super_resolution.onnx
class EsrganUpscaler : public AiModelLoader {
public:
    EsrganUpscaler(bool useDirectML)
        : AiModelLoader(L"super_resolution.onnx", useDirectML) {}

    // Runs inference to upscale an RGB frame
    void UpscaleFrame(const void* rgbFrameData, void* outHighResData) {
        // ... tensor binding and execution ...
    }
};

} // namespace AI
} // namespace NexVR
