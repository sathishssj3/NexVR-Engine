#include "ai/midas_inpainter.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <onnxruntime_cxx_api.h>

namespace NexVR {
namespace AI {

MidasInpainter::MidasInpainter(bool useDirectML, int deviceId, const std::wstring& modelPath)
    : AiModelLoader(modelPath, useDirectML, deviceId) {
}

void MidasInpainter::ResetTemporalHistory() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_historyDepth.clear();
    m_historyWidth = 0;
    m_historyHeight = 0;
}

bool MidasInpainter::GenerateDepthMapFromRGBA(const uint8_t* rgbaPacked, float* outDepth, int width, int height) {
    if (!rgbaPacked || !outDepth || width <= 0 || height <= 0) return false;

    size_t planeSize = static_cast<size_t>(width) * height;
    std::vector<float> planarRGB(3 * planeSize);

    float* rPlane = planarRGB.data();
    float* gPlane = planarRGB.data() + planeSize;
    float* bPlane = planarRGB.data() + 2 * planeSize;

    // Convert packed RGBA8 [0..255] to planar RGB [0.0..1.0]
    for (size_t i = 0; i < planeSize; ++i) {
        size_t srcIdx = i * 4;
        rPlane[i] = static_cast<float>(rgbaPacked[srcIdx]) / 255.0f;
        gPlane[i] = static_cast<float>(rgbaPacked[srcIdx + 1]) / 255.0f;
        bPlane[i] = static_cast<float>(rgbaPacked[srcIdx + 2]) / 255.0f;
    }

    return GenerateDepthMap(planarRGB.data(), outDepth, width, height);
}

bool MidasInpainter::GenerateDepthMap(const float* rgbPlanar, float* outDepth, int width, int height) {
    if (!rgbPlanar || !outDepth || width <= 0 || height <= 0) return false;

    std::lock_guard<std::mutex> lock(m_mutex);
    size_t planeSize = static_cast<size_t>(width) * height;

    // Fast fallback if ONNX session is uninitialized or in simulated mode
    if (!m_session || m_isSimulated) {
        ComputeHeuristicDepthFallback(rgbPlanar, outDepth, width, height);
    } else {
        try {
            int64_t inputDims[4] = { 1, 3, height, width };
            int64_t outputDims[4] = { 1, 1, height, width };

            Ort::MemoryInfo memInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

            Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
                memInfo, const_cast<float*>(rgbPlanar), 3 * planeSize, inputDims, 4);

            Ort::Value outputTensor = Ort::Value::CreateTensor<float>(
                memInfo, outDepth, planeSize, outputDims, 4);

            const char* inputNames[] = { "image", "input", "input_1" };
            const char* outputNames[] = { "depth", "output", "output_1" };

            // Probe input and output names from the active model session
            Ort::AllocatorWithDefaultOptions allocator;
            auto inputNameAllocated = m_session->GetInputNameAllocated(0, allocator);
            auto outputNameAllocated = m_session->GetOutputNameAllocated(0, allocator);

            const char* actualInputNames[] = { inputNameAllocated.get() };
            const char* actualOutputNames[] = { outputNameAllocated.get() };

            m_session->Run(
                Ort::RunOptions{nullptr},
                actualInputNames, &inputTensor, 1,
                actualOutputNames, &outputTensor, 1
            );
        } catch (const Ort::Exception& ex) {
            std::cerr << "[Stereix DirectML Depth] Inference warning: " << ex.what() 
                      << ". Engaging robust depth heuristic fallback.\n";
            ComputeHeuristicDepthFallback(rgbPlanar, outDepth, width, height);
        } catch (...) {
            std::cerr << "[Stereix DirectML Depth] Unknown exception during neural depth synthesis. Engaging fallback.\n";
            ComputeHeuristicDepthFallback(rgbPlanar, outDepth, width, height);
        }
    }

    // Edge-preserving bilateral filter pass to prevent stereo shimmer at silhouettes
    if (m_enableBilateral) {
        std::vector<float> filteredDepth(planeSize);
        ApplyBilateralFilter(outDepth, rgbPlanar, filteredDepth.data(), width, height);
        std::memcpy(outDepth, filteredDepth.data(), planeSize * sizeof(float));
    }

    // Temporal smoothing pass across consecutive frames
    if (m_temporalSmoothingFactor > 0.0f) {
        ApplyTemporalSmoothing(outDepth, width, height);
    }

    return true;
}

void MidasInpainter::ComputeHeuristicDepthFallback(const float* rgbPlanar, float* outDepth, int width, int height) {
    size_t planeSize = static_cast<size_t>(width) * height;
    const float* rPlane = rgbPlanar;
    const float* gPlane = rgbPlanar + planeSize;
    const float* bPlane = rgbPlanar + 2 * planeSize;

    for (int y = 0; y < height; ++y) {
        // Vertical perspective gradient: bottom of the screen is close (near depth), top is distant (sky)
        float verticalDepth = 0.2f + 0.7f * (1.0f - (static_cast<float>(y) / static_cast<float>(height)));

        for (int x = 0; x < width; ++x) {
            size_t idx = y * width + x;
            // Perceptual luminance calculation: Y = 0.299 R + 0.587 G + 0.114 B
            float lum = 0.299f * rPlane[idx] + 0.587f * gPlane[idx] + 0.114f * bPlane[idx];

            // Blend vertical perspective prior with local luminance contrast
            float d = verticalDepth + 0.1f * lum;
            outDepth[idx] = std::clamp(d, 0.05f, 1.0f);
        }
    }
}

void MidasInpainter::ApplyBilateralFilter(const float* inputDepth, const float* rgbPlanar, float* outputDepth, int width, int height) {
    size_t planeSize = static_cast<size_t>(width) * height;
    const float* rPlane = rgbPlanar;
    const float* gPlane = rgbPlanar + planeSize;
    const float* bPlane = rgbPlanar + 2 * planeSize;

    const float sigmaSpatialSq = 2.0f * 1.5f * 1.5f;
    const float sigmaColorSq = 2.0f * 0.1f * 0.1f;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t centerIdx = y * width + x;
            float centerD = inputDepth[centerIdx];
            float centerR = rPlane[centerIdx];
            float centerG = gPlane[centerIdx];
            float centerB = bPlane[centerIdx];

            float sumDepth = 0.0f;
            float sumWeights = 0.0f;

            for (int dy = -2; dy <= 2; ++dy) {
                int ny = std::clamp(y + dy, 0, height - 1);
                for (int dx = -2; dx <= 2; ++dx) {
                    int nx = std::clamp(x + dx, 0, width - 1);
                    size_t neighborIdx = ny * width + nx;

                    float spatialDistSq = static_cast<float>(dx * dx + dy * dy);
                    float spatialWeight = std::exp(-spatialDistSq / sigmaSpatialSq);

                    float diffR = centerR - rPlane[neighborIdx];
                    float diffG = centerG - gPlane[neighborIdx];
                    float diffB = centerB - bPlane[neighborIdx];
                    float colorDistSq = diffR * diffR + diffG * diffG + diffB * diffB;
                    float colorWeight = std::exp(-colorDistSq / sigmaColorSq);

                    float w = spatialWeight * colorWeight;
                    sumDepth += inputDepth[neighborIdx] * w;
                    sumWeights += w;
                }
            }

            outputDepth[centerIdx] = (sumWeights > 0.0001f) ? (sumDepth / sumWeights) : centerD;
        }
    }
}

void MidasInpainter::ApplyTemporalSmoothing(float* currentDepth, int width, int height) {
    size_t planeSize = static_cast<size_t>(width) * height;

    if (m_historyWidth != width || m_historyHeight != height || m_historyDepth.size() != planeSize) {
        m_historyDepth.assign(currentDepth, currentDepth + planeSize);
        m_historyWidth = width;
        m_historyHeight = height;
        return;
    }

    float alpha = 1.0f - m_temporalSmoothingFactor; // Weight on new frame
    for (size_t i = 0; i < planeSize; ++i) {
        float smoothed = alpha * currentDepth[i] + (1.0f - alpha) * m_historyDepth[i];
        currentDepth[i] = smoothed;
        m_historyDepth[i] = smoothed;
    }
}

} // namespace AI
} // namespace NexVR
