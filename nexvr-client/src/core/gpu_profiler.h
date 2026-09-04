#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <array>
#include <mutex>
#include <cstdint>

namespace vrinject {

enum class GpuSegment {
    StereoCompute = 0,
    TextureCopies,
    OpenXrSubmission,
    COUNT
};

struct GpuTimingResult {
    double ms;
    bool valid;
};

class GpuProfiler {
public:
    GpuProfiler();
    ~GpuProfiler() = default;

    bool Initialize(ID3D11Device* device);
    
    // Call once per frame
    void BeginFrame(ID3D11DeviceContext* ctx, uint64_t frameIndex);
    void EndFrame(ID3D11DeviceContext* ctx);

    // Call to measure specific segments
    void BeginSegment(ID3D11DeviceContext* ctx, GpuSegment segment);
    void EndSegment(ID3D11DeviceContext* ctx, GpuSegment segment);

    // Expose raw query for scenarios where we can't easily wrap (e.g., inside other classes)
    ID3D11Query* GetInternalQueryStart(GpuSegment segment);
    ID3D11Query* GetInternalQueryEnd(GpuSegment segment);

    // Get the most recently resolved results
    bool TryGetTimings(uint64_t& outFrameIndex, std::array<GpuTimingResult, static_cast<uint32_t>(GpuSegment::COUNT)>& outResults);

private:
    static constexpr uint32_t QUERY_LATENCY_FRAMES = 3;

    struct FrameQuerySet {
        Microsoft::WRL::ComPtr<ID3D11Query> disjointQuery;
        std::array<Microsoft::WRL::ComPtr<ID3D11Query>, static_cast<uint32_t>(GpuSegment::COUNT)> timestampStart;
        std::array<Microsoft::WRL::ComPtr<ID3D11Query>, static_cast<uint32_t>(GpuSegment::COUNT)> timestampEnd;
        uint64_t frameIndex = 0;
        bool issued = false;
    };

    std::array<FrameQuerySet, QUERY_LATENCY_FRAMES> m_queryPool;
    uint32_t m_currentSlot = 0;
    bool m_initialized = false;

    std::mutex m_resultsMutex;
    uint64_t m_lastResolvedFrame = 0;
    std::array<GpuTimingResult, static_cast<uint32_t>(GpuSegment::COUNT)> m_lastResults;
};

} // namespace vrinject
