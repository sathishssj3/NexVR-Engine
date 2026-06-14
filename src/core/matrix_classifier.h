#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include <vector>
#include <cstdint>
#include <mutex>

namespace vrinject {

// Candidate camera matrix with metadata
struct MatrixCandidate {
    DirectX::XMFLOAT4X4 matrix;
    DirectX::XMFLOAT4X4 previousMatrix;
    void*    bufferAddress;   // CPU-side memory address
    uint32_t updateCount;     // times updated this session
    float    confidence;      // 0.0 - 1.0 heuristic score
    bool     locked;          // true = confirmed camera matrix
    float    deltaScore;      // Accumulated score for active movement
};

class MatrixClassifier {
public:
    static MatrixClassifier& Get() {
        static MatrixClassifier instance;
        return instance;
    }

    // Called every time a constant buffer is written
    void OnConstantBufferUpdate(
        const void* pData,
        size_t      byteSize,
        void*       bufferAddress);

    // Returns the best camera matrix candidate found so far
    bool GetCameraMatrix(DirectX::XMFLOAT4X4& outMatrix) const;

    // Lock in the confirmed camera matrix address
    void LockCameraMatrix(void* address);

    // Overwrite the game's camera matrix with VR pose
    bool OverwriteCameraMatrix(
        const DirectX::XMFLOAT4X4& vrMatrix);

    void Reset();

    // Thread safety — render thread vs OpenXR thread
    mutable std::mutex m_mutex;

private:
    MatrixClassifier() = default;

    std::vector<MatrixCandidate> m_candidates;
    void*    m_lockedAddress  = nullptr;
    uint32_t m_frameCount     = 0;
    uint32_t m_lockedFrames   = 0;

    // Heuristic scoring
    float ScoreMatrix(const DirectX::XMFLOAT4X4& m) const;
    bool  LooksLikeProjectionMatrix(
              const DirectX::XMFLOAT4X4& m) const;
    bool  LooksLikeViewMatrix(
              const DirectX::XMFLOAT4X4& m) const;
    float CalculateDelta(const DirectX::XMFLOAT4X4& m1, const DirectX::XMFLOAT4X4& m2) const;
};

} // namespace vrinject
