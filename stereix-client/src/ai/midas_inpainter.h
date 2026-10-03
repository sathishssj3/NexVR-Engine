#pragma once

#include "ai/ai_model_loader.h"
#include <vector>
#include <mutex>
#include <cstdint>

namespace NexVR {
namespace AI {

/**
 * @brief Real-Time DirectML AI Stereo Depth Synthesizer
 *
 * Infers metric or relative scene depth from monocular 2D RGB frames
 * using hardware-accelerated DirectML ONNX models (e.g. MiDaS / Depth-Anything / DPT).
 * Includes temporal exponential smoothing (EMA) and edge-preserving bilateral filtering
 * to prevent stereo shimmer in VR headsets.
 */
class MidasInpainter : public AiModelLoader {
public:
    MidasInpainter(bool useDirectML = true, int deviceId = 0, const std::wstring& modelPath = L"models/depth_inpainter.onnx");
    ~MidasInpainter() override = default;

    /**
     * @brief Generates a depth map from a planar RGB buffer [3 * width * height], normalized [0.0, 1.0].
     * @param rgbPlanar Float array of size 3 * width * height (R plane, G plane, B plane).
     * @param outDepth Float array of size width * height receiving normalized depth [0.0, 1.0].
     * @param width Frame width.
     * @param height Frame height.
     * @return true if successfully inferred or filtered; false on invalid inputs.
     */
    bool GenerateDepthMap(const float* rgbPlanar, float* outDepth, int width, int height);

    /**
     * @brief Generates a depth map from standard packed 32-bit RGBA8 screen pixels.
     * @param rgbaPacked Byte array of size 4 * width * height.
     * @param outDepth Float array of size width * height receiving normalized depth [0.0, 1.0].
     * @param width Frame width.
     * @param height Frame height.
     */
    bool GenerateDepthMapFromRGBA(const uint8_t* rgbaPacked, float* outDepth, int width, int height);

    /**
     * @brief Configures temporal exponential moving average (EMA) smoothing.
     * @param factor Smoothing factor in [0.0, 1.0]. 0.0 = no history, 0.8 = strong anti-shimmer.
     */
    void SetTemporalSmoothing(float factor) {
        m_temporalSmoothingFactor = (factor < 0.0f) ? 0.0f : ((factor > 0.95f) ? 0.95f : factor);
    }

    /**
     * @brief Enables or disables edge-preserving bilateral filtering pass.
     */
    void SetBilateralFiltering(bool enable) {
        m_enableBilateral = enable;
    }

    /**
     * @brief Clears accumulated temporal history buffer (e.g. on scene transitions or camera cuts).
     */
    void ResetTemporalHistory();

private:
    std::mutex m_mutex;
    float m_temporalSmoothingFactor{0.35f};
    bool m_enableBilateral{true};

    std::vector<float> m_historyDepth;
    int m_historyWidth{0};
    int m_historyHeight{0};

    void ApplyBilateralFilter(const float* inputDepth, const float* rgbPlanar, float* outputDepth, int width, int height);
    void ApplyTemporalSmoothing(float* currentDepth, int width, int height);
    void ComputeHeuristicDepthFallback(const float* rgbPlanar, float* outDepth, int width, int height);
};

} // namespace AI
} // namespace NexVR
