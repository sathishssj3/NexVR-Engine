#include "matrix_classifier.h"

// Note: You need to install the ONNX Runtime C++ NuGet package or download the binaries.
// In CMake: target_link_libraries(vrinject PRIVATE onnxruntime)
#include <onnxruntime_cxx_api.h>
#include <cmath>
#include <algorithm>
#include <iostream>

namespace vrinject {
namespace ai {

MatrixClassifier::MatrixClassifier() = default;
MatrixClassifier::~MatrixClassifier() = default;

bool MatrixClassifier::Initialize(const std::wstring& modelPath) {
    try {
        // Initialize ONNX Runtime environment
        m_env = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "VRInject_AI");
        
        Ort::SessionOptions sessionOptions;
        sessionOptions.SetIntraOpNumThreads(1); // Keep it lightweight
        sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        // Load the model
        m_session = std::make_unique<Ort::Session>(*m_env, modelPath.c_str(), sessionOptions);
        
        m_memoryInfo = std::make_unique<Ort::MemoryInfo>(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault));
        
        return true;
    } catch (const Ort::Exception& e) {
        std::cerr << "ONNX Runtime initialization failed: " << e.what() << std::endl;
        return false;
    }
}

MatrixPrediction MatrixClassifier::Classify(const void* pData) {
    if (!m_session || !pData) {
        return { MatrixType::Unknown, 0.0f };
    }

    // Input shape is [1, 16]
    std::vector<int64_t> inputDims = { 1, 16 };
    size_t inputTensorSize = 16;

    // The data is 16 floats (64 bytes). We cast it to a non-const pointer for ONNX, 
    // but we promise not to modify it (the model only reads).
    float* inputData = const_cast<float*>(reinterpret_cast<const float*>(pData));

    Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
        *m_memoryInfo, inputData, inputTensorSize, inputDims.data(), inputDims.size());

    const char* inputNames[] = { "input" };
    const char* outputNames[] = { "output" };

    try {
        // Run inference
        auto outputTensors = m_session->Run(
            Ort::RunOptions{nullptr}, 
            inputNames, &inputTensor, 1, 
            outputNames, 1);

        // Process output [1, 5] logits
        float* floatArr = outputTensors.front().GetTensorMutableData<float>();
        
        // Softmax to get probabilities
        float maxVal = *std::max_element(floatArr, floatArr + 5);
        float sumExp = 0.0f;
        float probs[5];
        
        for (int i = 0; i < 5; ++i) {
            probs[i] = std::exp(floatArr[i] - maxVal);
            sumExp += probs[i];
        }
        
        int bestClass = 0;
        float bestProb = 0.0f;
        
        for (int i = 0; i < 5; ++i) {
            probs[i] /= sumExp;
            if (probs[i] > bestProb) {
                bestProb = probs[i];
                bestClass = i;
            }
        }

        return { static_cast<MatrixType>(bestClass), bestProb };

    } catch (const Ort::Exception& e) {
        std::cerr << "Inference failed: " << e.what() << std::endl;
        return { MatrixType::Unknown, 0.0f };
    }
}

std::vector<std::pair<int, MatrixPrediction>> MatrixClassifier::ScanBuffer(const void* pBuffer, size_t bufferSize) {
    std::vector<std::pair<int, MatrixPrediction>> results;
    
    if (!pBuffer || bufferSize < 64) return results;

    const char* bytes = reinterpret_cast<const char*>(pBuffer);
    
    // Constant buffers usually align variables to 16 bytes.
    // A 4x4 matrix is 64 bytes. We slide a 64-byte window by 16 bytes.
    for (size_t offset = 0; offset + 64 <= bufferSize; offset += 16) {
        MatrixPrediction pred = Classify(bytes + offset);
        
        // Only return strong predictions to reduce noise
        if (pred.type != MatrixType::Unknown && pred.confidence > 0.8f) {
            results.push_back({ static_cast<int>(offset), pred });
        }
    }
    
    return results;
}

} // namespace ai
} // namespace vrinject
