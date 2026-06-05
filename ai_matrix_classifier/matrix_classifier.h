#pragma once
#include <string>
#include <vector>
#include <memory>

// Forward declarations for ONNX Runtime to avoid exposing headers to the rest of the project
namespace Ort {
    class Env;
    class Session;
    class MemoryInfo;
}

namespace vrinject {
namespace ai {

enum class MatrixType {
    Unknown = 0,
    View = 1,
    PerspectiveProjection = 2,
    OrthographicProjection = 3,
    MVP = 4
};

struct MatrixPrediction {
    MatrixType type;
    float confidence; // 0.0 to 1.0
};

class MatrixClassifier {
public:
    MatrixClassifier();
    ~MatrixClassifier();

    // Initialize the ONNX Runtime session with the exported model
    bool Initialize(const std::wstring& modelPath);

    // Predict the type of a 64-byte chunk (16 floats)
    // pData must point to at least 64 bytes of valid memory
    MatrixPrediction Classify(const void* pData);

    // Scan a large constant buffer and return predictions for all 64-byte aligned chunks
    std::vector<std::pair<int, MatrixPrediction>> ScanBuffer(const void* pBuffer, size_t bufferSize);

private:
    std::unique_ptr<Ort::Env> m_env;
    std::unique_ptr<Ort::Session> m_session;
    std::unique_ptr<Ort::MemoryInfo> m_memoryInfo;
};

} // namespace ai
} // namespace vrinject
