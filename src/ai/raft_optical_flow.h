#pragma once

#include "ai_model_loader.h"

namespace NexVR {
namespace AI {

// Wraps optical_flow_raft.onnx
class RaftOpticalFlow : public AiModelLoader {
public:
    RaftOpticalFlow(bool useDirectML)
        : AiModelLoader(L"optical_flow_raft.onnx", useDirectML) {}

    // Runs inference on two consecutive frames to generate motion vectors
    void ComputeMotionVectors(const void* frame1, const void* frame2, void* outFlowMap) {
        // ... tensor binding and execution ...
    }
};

} // namespace AI
} // namespace NexVR
