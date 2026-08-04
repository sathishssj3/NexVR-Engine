#pragma once

#include "ai_model_loader.h"
#include <vector>

namespace NexVR {
namespace AI {

struct BoundingBox {
    float x1, y1, x2, y2;
    float confidence;
    int classId;
};

// Wraps ui_synthesizer.onnx
class YoloSynthesizer : public AiModelLoader {
public:
    YoloSynthesizer(bool useDirectML)
        : AiModelLoader(L"ui_synthesizer.onnx", useDirectML) {}

    // Runs inference on a 640x640 frame and returns UI bounding boxes
    std::vector<BoundingBox> DetectUIElements(const void* rgbFrameData) {
        // ... tensor binding ...
        // ... post-processing NMS ...
        return {}; // Stub
    }
};

} // namespace AI
} // namespace NexVR
