#pragma once
#include <string>
#include <vector>

namespace vrinject {
namespace ai {

enum class MatrixType { Unknown, PerspectiveProjection, View, World };

struct Prediction {
    MatrixType type;
    float confidence;
};

class MatrixClassifier {
public:
    bool Initialize(const std::wstring& modelPath);
    std::vector<std::pair<int, Prediction>> ScanBuffer(const void* data, size_t size);
    
private:
    bool m_initialized = false;
};

} // namespace ai
} // namespace vrinject
