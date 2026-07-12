#include "universal_scanner.h"
#include "../logger.h"
#include "../matrix_classifier.h"
#include "../../rendering/openxr_manager.h"
#include "../config_manager.h"
#include <cstring>
#include <cmath>

namespace vrinject {
namespace engine_scanners {

bool UniversalScanner::Initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized) return true;

    LOG_INFO("UniversalScanner: Initializing AI-driven heuristic scanner.");
    
    // Matrix Classifier runs implicitly.

    m_lockedBufferSize = 0;
    m_lockedMatrixOffset = 0;
    m_hasLock = false;

    m_initialized = true;
    return true;
}

void UniversalScanner::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_initialized = false;
    m_hasLock = false;
}

bool UniversalScanner::ProcessConstantBuffer(void* data, size_t size) {
    if (!m_initialized || !data || size < 64) {
        return false;
    }

    // Optimization: Constant buffers for camera matrices are typically exactly 
    // 64 (View), 128 (View + Proj), 192, or 256 bytes in modern engines.
    // Scanning giant buffers (e.g., bone matrices or lights) is a waste of CPU.
    if (size > 512) {
        return false;
    }

    // Fast path: Check if we have a locked buffer signature
    // Only hold lock for reading the cached values
    bool hasLock = false;
    uint32_t lockedBufferSize = 0;
    uint32_t lockedMatrixOffset = 0;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        hasLock = m_hasLock;
        lockedBufferSize = m_lockedBufferSize;
        lockedMatrixOffset = m_lockedMatrixOffset;
    }

    uint32_t matrixOffset = 0;
    bool foundInActiveLock = false;

    // Check if MatrixClassifier has locked onto an address residing in this buffer first
    void* lockedAddr = MatrixClassifier::Get().GetLockedAddress();
    if (lockedAddr) {
        uintptr_t addrVal = reinterpret_cast<uintptr_t>(lockedAddr);
        uintptr_t dataVal = reinterpret_cast<uintptr_t>(data);
        if (addrVal >= dataVal && addrVal + 64 <= dataVal + size) {
            matrixOffset = static_cast<uint32_t>(addrVal - dataVal);
            foundInActiveLock = true;
            
            // Sync cache if needed
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_hasLock || m_lockedBufferSize != size || m_lockedMatrixOffset != matrixOffset) {
                m_lockedBufferSize = static_cast<uint32_t>(size);
                m_lockedMatrixOffset = matrixOffset;
                m_hasLock = true;
                LOG_INFO("UniversalScanner: Sync locked onto Camera Matrix (Size: %zu, Offset: %u)", size, matrixOffset);
            }
        }
    }

    if (!foundInActiveLock) {
        if (hasLock && size == lockedBufferSize) {
            // Fast path: use cached offset
            matrixOffset = lockedMatrixOffset;
        } else {
            // Slow path: Ask the AI to classify 64-byte chunks in this buffer
            // Do NOT hold our mutex while calling MatrixClassifier to avoid deadlock
            if (!AnalyzeBufferForMatrices(data, size, matrixOffset)) {
                return false;
            }

            // We found a view matrix! Update the cached signature
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_lockedBufferSize = static_cast<uint32_t>(size);
                m_lockedMatrixOffset = matrixOffset;
                m_hasLock = true;
                LOG_INFO("UniversalScanner: Locked onto Camera Matrix (Size: %zu, Offset: %u)", size, matrixOffset);
            }
        }
    }

    // --- Stereoscopic Injection ---
    auto* openxr = OpenXRManager::GetInstance();
    if (openxr && openxr->IsSessionRunning()) {
        // We have found a View Matrix (4x4 floats) at 'matrixOffset' inside 'data'.
        // A View matrix describes the camera's rotation and position.
        float* viewMatrix = reinterpret_cast<float*>(static_cast<uint8_t*>(data) + matrixOffset);

        const XrPosef& headPose = openxr->GetLatestHeadPose();

        // 1. Convert OpenXR pose to translation offset
        // OpenXR is meters, right-handed. 
        // We assume the game is using centimeters or meters. Let's scale to game units.
        // For a universal scanner, we use a configurable scale factor.
        float scale = ConfigManager::GetInstance().GetConfig().vrScaleFactor; // Game units per meter
        
        // Very basic positional injection: 
        // View matrices typically store translation in the right column or bottom row.
        // We'll apply a simple translation offset based on the VR head position.
        // In DirectX (Row-Major), translation is typically at indices [12, 13, 14].
        
        viewMatrix[12] += headPose.position.x * scale;
        viewMatrix[13] += headPose.position.y * scale;
        viewMatrix[14] += headPose.position.z * scale;
        
        // TODO: Full rotational injection using headPose.orientation would require
        // decomposing the view matrix, injecting the rotation, and rebuilding it.

        return true;
    }

    return false;
}

bool UniversalScanner::AnalyzeBufferForMatrices(void* data, size_t size, uint32_t& outOffset) {
    uint8_t* byteData = static_cast<uint8_t*>(data);
    
    // We advance by 64 bytes at a time (assuming strict alignment for matrices)
    // Pass the exact unique address of the matrix in memory to OnConstantBufferUpdate
    for (size_t offset = 0; offset + 64 <= size; offset += 64) {
        MatrixClassifier::Get().OnConstantBufferUpdate(byteData + offset, 64, byteData + offset);
    }
    
    // Check if the classifier has locked onto an address inside this buffer
    void* lockedAddr = MatrixClassifier::Get().GetLockedAddress();
    if (lockedAddr) {
        uintptr_t addrVal = reinterpret_cast<uintptr_t>(lockedAddr);
        uintptr_t dataVal = reinterpret_cast<uintptr_t>(data);
        if (addrVal >= dataVal && addrVal + 64 <= dataVal + size) {
            outOffset = static_cast<uint32_t>(addrVal - dataVal);
            return true;
        }
    }
    
    return false;
}

} // namespace engine_scanners
} // namespace vrinject
