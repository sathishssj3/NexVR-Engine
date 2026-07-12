#pragma once

#include <vector>
#include <mutex>
#include <cstdint>
#include <DirectXMath.h>

namespace vrinject {
namespace engine_scanners {

// UniversalScanner uses AI heuristics to locate View and Projection matrices
// in GPU constant buffers when specific engine signatures (Unreal/Unity) fail.
class UniversalScanner {
public:
    static UniversalScanner& Get() {
        static UniversalScanner instance;
        return instance;
    }

    bool Initialize();
    void Shutdown();

    // Analyze a raw buffer (e.g., from DX11 UpdateSubresource or DX12 Map)
    // Returns true if a stereoscopic modification was applied to the buffer in-place.
    bool ProcessConstantBuffer(void* data, size_t size);

private:
    UniversalScanner() = default;
    ~UniversalScanner() = default;

    bool m_initialized = false;
    std::mutex m_mutex;

    // Track the size/signature of the buffer that consistently has the camera matrix
    size_t m_lockedBufferSize = 0;
    uint32_t m_lockedMatrixOffset = 0;
    bool m_hasLock = false;

    // Analyze 64-byte chunks of the buffer using the AI Matrix Classifier
    bool AnalyzeBufferForMatrices(void* data, size_t size, uint32_t& outOffset);
};

} // namespace engine_scanners
} // namespace vrinject
