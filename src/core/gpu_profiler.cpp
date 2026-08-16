#include "core/gpu_profiler.h"
#include "core/logger.h"

namespace vrinject {

GpuProfiler::GpuProfiler() {
    for (auto& r : m_lastResults) {
        r.ms = 0.0;
        r.valid = false;
    }
}

bool GpuProfiler::Initialize(ID3D11Device* device) {
    if (!device) return false;

    D3D11_QUERY_DESC disjointDesc = {};
    disjointDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;

    D3D11_QUERY_DESC timestampDesc = {};
    timestampDesc.Query = D3D11_QUERY_TIMESTAMP;

    for (uint32_t i = 0; i < QUERY_LATENCY_FRAMES; ++i) {
        HRESULT hr = device->CreateQuery(&disjointDesc, m_queryPool[i].disjointQuery.ReleaseAndGetAddressOf());
        if (FAILED(hr)) {
            LOG_ERROR("Failed to create disjoint query");
            return false;
        }

        for (uint32_t j = 0; j < static_cast<uint32_t>(GpuSegment::COUNT); ++j) {
            hr = device->CreateQuery(&timestampDesc, m_queryPool[i].timestampStart[j].ReleaseAndGetAddressOf());
            if (FAILED(hr)) return false;
            
            hr = device->CreateQuery(&timestampDesc, m_queryPool[i].timestampEnd[j].ReleaseAndGetAddressOf());
            if (FAILED(hr)) return false;
        }
    }

    m_initialized = true;
    return true;
}

void GpuProfiler::BeginFrame(ID3D11DeviceContext* ctx, uint64_t frameIndex) {
    if (!m_initialized) return;

    FrameQuerySet& current = m_queryPool[m_currentSlot];
    if (current.issued) {
        // We overflowed our ring buffer without resolving. Force clear.
        current.issued = false;
    }

    ctx->Begin(current.disjointQuery.Get());
    current.frameIndex = frameIndex;
    current.issued = true;
}

void GpuProfiler::EndFrame(ID3D11DeviceContext* ctx) {
    if (!m_initialized) return;

    FrameQuerySet& current = m_queryPool[m_currentSlot];
    if (!current.issued) return;

    ctx->End(current.disjointQuery.Get());

    // Move to next slot
    m_currentSlot = (m_currentSlot + 1) % QUERY_LATENCY_FRAMES;

    // Resolve N-latency frame
    uint32_t resolveIdx = (m_currentSlot + 1) % QUERY_LATENCY_FRAMES; 
    // E.g., if latency=3, current=1, we resolve (1+1)%3 = 2, which is the oldest.
    
    FrameQuerySet& resolveSet = m_queryPool[resolveIdx];

    if (resolveSet.issued) {
        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjointData;
        HRESULT hr = ctx->GetData(resolveSet.disjointQuery.Get(), &disjointData, sizeof(disjointData), D3D11_ASYNC_GETDATA_DONOTFLUSH);
        
        std::array<GpuTimingResult, static_cast<uint32_t>(GpuSegment::COUNT)> results = {};
        bool allValid = (hr == S_OK) && !disjointData.Disjoint;

        if (allValid) {
            uint64_t freq = disjointData.Frequency;
            for (uint32_t i = 0; i < static_cast<uint32_t>(GpuSegment::COUNT); ++i) {
                UINT64 startTime = 0, endTime = 0;
                hr = ctx->GetData(resolveSet.timestampStart[i].Get(), &startTime, sizeof(UINT64), D3D11_ASYNC_GETDATA_DONOTFLUSH);
                if (hr != S_OK) { allValid = false; break; }
                
                hr = ctx->GetData(resolveSet.timestampEnd[i].Get(), &endTime, sizeof(UINT64), D3D11_ASYNC_GETDATA_DONOTFLUSH);
                if (hr != S_OK) { allValid = false; break; }

                if (endTime >= startTime) {
                    double ms = (double)(endTime - startTime) / (double)freq * 1000.0;
                    results[i] = { ms, true };
                } else {
                    results[i] = { 0.0, false };
                }
            }
        }

        if (allValid) {
            std::lock_guard<std::mutex> lock(m_resultsMutex);
            m_lastResults = results;
            m_lastResolvedFrame = resolveSet.frameIndex;
        }

        // Whether we succeeded or failed (e.g. data not ready), we free the slot so we don't stall.
        // If data wasn't ready because the GPU is deeply backed up, we just lose this frame's profiling.
        resolveSet.issued = false;
    }
}

void GpuProfiler::BeginSegment(ID3D11DeviceContext* ctx, GpuSegment segment) {
    if (!m_initialized) return;
    FrameQuerySet& current = m_queryPool[m_currentSlot];
    if (current.issued) {
        ctx->End(current.timestampStart[static_cast<uint32_t>(segment)].Get());
    }
}

void GpuProfiler::EndSegment(ID3D11DeviceContext* ctx, GpuSegment segment) {
    if (!m_initialized) return;
    FrameQuerySet& current = m_queryPool[m_currentSlot];
    if (current.issued) {
        ctx->End(current.timestampEnd[static_cast<uint32_t>(segment)].Get());
    }
}

ID3D11Query* GpuProfiler::GetInternalQueryStart(GpuSegment segment) {
    if (!m_initialized) return nullptr;
    return m_queryPool[m_currentSlot].timestampStart[static_cast<uint32_t>(segment)].Get();
}

ID3D11Query* GpuProfiler::GetInternalQueryEnd(GpuSegment segment) {
    if (!m_initialized) return nullptr;
    return m_queryPool[m_currentSlot].timestampEnd[static_cast<uint32_t>(segment)].Get();
}

bool GpuProfiler::TryGetTimings(uint64_t& outFrameIndex, std::array<GpuTimingResult, static_cast<uint32_t>(GpuSegment::COUNT)>& outResults) {
    std::lock_guard<std::mutex> lock(m_resultsMutex);
    if (m_lastResolvedFrame == 0) return false;
    
    outFrameIndex = m_lastResolvedFrame;
    outResults = m_lastResults;
    return true;
}

} // namespace vrinject
