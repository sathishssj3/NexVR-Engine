#include "matrix_classifier.h"
#include "logger.h"
#include <cmath>
#include <algorithm>
#include <cstring>

namespace vrinject {

void MatrixClassifier::OnConstantBufferUpdate(const void* pData, size_t byteSize, void* bufferAddress) {
    if (byteSize < sizeof(DirectX::XMFLOAT4X4)) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    
    // We parse the constant buffer data looking for any 4x4 float matrices
    size_t numMatrices = byteSize / sizeof(DirectX::XMFLOAT4X4);
    const DirectX::XMFLOAT4X4* matrices = static_cast<const DirectX::XMFLOAT4X4*>(pData);

    for (size_t i = 0; i < numMatrices; ++i) {
        const DirectX::XMFLOAT4X4& m = matrices[i];
        float score = ScoreMatrix(m);

        if (score > 0.0f) {
            void* matrixAddr = static_cast<uint8_t*>(bufferAddress) + (i * sizeof(DirectX::XMFLOAT4X4));
            
            // Find existing candidate or add new
            auto it = std::find_if(m_candidates.begin(), m_candidates.end(),
                [matrixAddr](const MatrixCandidate& c) { return c.bufferAddress == matrixAddr; });

            if (it != m_candidates.end()) {
                it->previousMatrix = it->matrix;
                it->matrix = m;
                it->updateCount++;
                
                // Track dynamic movement
                float delta = CalculateDelta(it->previousMatrix, it->matrix);
                if (delta > 0.00001f && delta < 5.0f) {
                    it->deltaScore += 0.02f; // Active camera
                } else if (delta == 0.0f) {
                    it->deltaScore -= 0.05f; // Static matrix (e.g. UI)
                }
                
                // Clamp delta score
                it->deltaScore = (std::max)(0.0f, (std::min)(0.5f, it->deltaScore));
                it->confidence = score + it->deltaScore;
            } else {
                m_candidates.push_back({m, m, matrixAddr, 1, score, false, 0.0f});
            }
        }
    }

    // Sort by confidence
    std::sort(m_candidates.begin(), m_candidates.end(),
        [](const MatrixCandidate& a, const MatrixCandidate& b) {
            return a.confidence > b.confidence;
        });

    // Keep top 8
    if (m_candidates.size() > 8) {
        m_candidates.resize(8);
    }

    // Auto-lock logic
    if (!m_candidates.empty() && !m_lockedAddress) {
        if (m_candidates[0].confidence >= 0.85f) {
            m_lockedFrames++;
            if (m_lockedFrames >= 10) {
                LockCameraMatrix(m_candidates[0].bufferAddress);
            }
        } else {
            m_lockedFrames = 0;
        }
    }
    
    m_frameCount++; // Simplistic frame tracking per update sequence
}

bool MatrixClassifier::GetCameraMatrix(DirectX::XMFLOAT4X4& outMatrix) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_candidates.empty()) {
        outMatrix = m_candidates[0].matrix;
        return true;
    }
    return false;
}

void MatrixClassifier::LockCameraMatrix(void* address) {
    if (m_lockedAddress != address) {
        m_lockedAddress = address;
        LOG_INFO("Camera Matrix auto-locked at address: %p", address);
        for (auto& c : m_candidates) {
            if (c.bufferAddress == address) {
                c.locked = true;
                break;
            }
        }
    }
}

bool MatrixClassifier::OverwriteCameraMatrix(const DirectX::XMFLOAT4X4& vrMatrix) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_lockedAddress) return false;
    
    // In a real DX11 hook, you'd write this directly to the mapped pointer
    std::memcpy(m_lockedAddress, &vrMatrix, sizeof(DirectX::XMFLOAT4X4));
    return true;
}

void MatrixClassifier::Reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_candidates.clear();
    m_lockedAddress = nullptr;
    m_frameCount = 0;
    m_lockedFrames = 0;
}

float MatrixClassifier::ScoreMatrix(const DirectX::XMFLOAT4X4& m) const {
    float score = 0.0f;
    
    if (LooksLikeProjectionMatrix(m)) score += 0.4f;
    if (LooksLikeViewMatrix(m)) score += 0.35f;

    // FOV and Aspect Ratio checking for projection matrices
    if (m._44 == 0.0f && m._22 > 0.0f) { // Likely projection
        float fovY = 2.0f * std::atan(1.0f / m._22) * (180.0f / 3.1415926535f);
        if (fovY >= 40.0f && fovY <= 120.0f) {
            score += 0.1f;
        } else {
            score -= 0.5f;
        }
        
        // Aspect Ratio checking
        if (m._11 > 0.0f) {
            float aspect = m._22 / m._11;
            // Typical aspect ratios: 16:9 (1.777), 16:10 (1.6), 21:9 (2.333), 4:3 (1.333)
            if (std::abs(aspect - 1.777f) < 0.05f || 
                std::abs(aspect - 1.6f) < 0.05f || 
                std::abs(aspect - 2.333f) < 0.05f ||
                std::abs(aspect - 1.333f) < 0.05f) {
                score += 0.2f;
            } else {
                score -= 0.1f;
            }
        }
    }

    return score;
}

bool MatrixClassifier::LooksLikeProjectionMatrix(const DirectX::XMFLOAT4X4& m) const {
    // m[3][3] == 0 (w-divide) and m[2][3] == 1 or -1
    return m._44 == 0.0f && (std::abs(m._34 - 1.0f) < 0.01f || std::abs(m._34 + 1.0f) < 0.01f);
}

bool MatrixClassifier::LooksLikeViewMatrix(const DirectX::XMFLOAT4X4& m) const {
    // Orthogonal check for upper 3x3
    DirectX::XMVECTOR r0 = DirectX::XMLoadFloat4(reinterpret_cast<const DirectX::XMFLOAT4*>(&m._11));
    DirectX::XMVECTOR r1 = DirectX::XMLoadFloat4(reinterpret_cast<const DirectX::XMFLOAT4*>(&m._21));
    DirectX::XMVECTOR r2 = DirectX::XMLoadFloat4(reinterpret_cast<const DirectX::XMFLOAT4*>(&m._31));

    float dot01 = DirectX::XMVectorGetX(DirectX::XMVector3Dot(r0, r1));
    if (std::abs(dot01) > 0.01f) return false;

    float len0 = DirectX::XMVectorGetX(DirectX::XMVector3Length(r0));
    if (std::abs(len0 - 1.0f) > 0.01f) return false;

    return true;
}

float MatrixClassifier::CalculateDelta(const DirectX::XMFLOAT4X4& m1, const DirectX::XMFLOAT4X4& m2) const {
    float delta = 0.0f;
    const float* a = &m1._11;
    const float* b = &m2._11;
    for (int i = 0; i < 16; ++i) {
        delta += std::abs(a[i] - b[i]);
    }
    return delta;
}

} // namespace vrinject
