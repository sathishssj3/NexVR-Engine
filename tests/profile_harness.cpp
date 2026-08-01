#include <iostream>
#include <vector>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include <DirectXMath.h>
#include "../src/core/matrix_classifier.h"

using namespace vrinject;
using namespace DirectX;

void GenerateProjMatrix(XMFLOAT4X4& out, float fov, float aspect, float nearZ, float farZ) {
    float f = 1.0f / std::tan(fov * 0.5f);
    out = {
        f / aspect, 0.0f, 0.0f, 0.0f,
        0.0f, f, 0.0f, 0.0f,
        0.0f, 0.0f, farZ / (farZ - nearZ), 1.0f,
        0.0f, 0.0f, -(farZ * nearZ) / (farZ - nearZ), 0.0f
    };
}

int main() {
    std::cout << std::unitbuf;
    std::cout << "==========================================\n";
    std::cout << " NexVR Engine - Matrix Overwrite Profiler\n";
    std::cout << "==========================================\n\n";

    try {
        MatrixClassifier& classifier = MatrixClassifier::Get();
    classifier.Reset();

    const size_t bufferSize = sizeof(XMFLOAT4X4) * 16; // Simulate a larger constant buffer
    std::vector<uint8_t> constantBuffer(bufferSize, 0);
    
    XMFLOAT4X4 engineProj;
    GenerateProjMatrix(engineProj, XMConvertToRadians(90.0f), 1.777f, 0.1f, 1000.0f);
    
    XMFLOAT4X4 vrProj;
    GenerateProjMatrix(vrProj, XMConvertToRadians(110.0f), 1.0f, 0.01f, 1000.0f);

    void* matrixAddr = constantBuffer.data() + sizeof(XMFLOAT4X4) * 4; // Offset in buffer

    // Warmup & Auto-lock phase
    std::cout << "Warming up and auto-locking matrix...\n";
    for (int i = 0; i < 20; ++i) {
        engineProj._41 += 0.001f; // SIMULATE MOVEMENT
        std::memcpy(matrixAddr, &engineProj, sizeof(XMFLOAT4X4));
        classifier.OnConstantBufferUpdate(constantBuffer.data(), bufferSize, constantBuffer.data());
    }

    if (!classifier.GetLockedAddress()) {
        std::cerr << "Failed to lock matrix during warmup!\n";
        return 1;
    }

    const int NUM_ITERATIONS = 10000;
    std::vector<long long> latencies;
    latencies.reserve(NUM_ITERATIONS);

    std::cout << "Running profiling loop (" << NUM_ITERATIONS << " iterations)...\n";

    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        // Simulate minor camera movement to keep it active (prevents deltaScore from dropping if we wanted to test that, but it's locked now)
        engineProj._41 += 0.001f;

        auto start = std::chrono::high_resolution_clock::now();

        // 1. CPU writes matrix
        std::memcpy(matrixAddr, &engineProj, sizeof(XMFLOAT4X4));

        // 2. Hook detects change (simulated via OnConstantBufferUpdate)
        classifier.OnConstantBufferUpdate(constantBuffer.data(), bufferSize, constantBuffer.data());

        // 3. Hook writes stereo matrix over it
        classifier.OverwriteCameraMatrix(vrProj, constantBuffer.data(), bufferSize);

        auto end = std::chrono::high_resolution_clock::now();
        
        long long durationNs = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        latencies.push_back(durationNs);
    }

    std::sort(latencies.begin(), latencies.end());

    long long p50 = latencies[NUM_ITERATIONS * 0.50];
    long long p90 = latencies[NUM_ITERATIONS * 0.90];
    long long p95 = latencies[NUM_ITERATIONS * 0.95];
    long long p99 = latencies[NUM_ITERATIONS * 0.99];
    long long maxLat = latencies.back();

    std::cout << "\n--- Latency Results ---\n";
    std::cout << "Median (50th):  " << p50 << " ns (" << p50 / 1000.0 << " us)\n";
    std::cout << "90th Percentile:" << p90 << " ns (" << p90 / 1000.0 << " us)\n";
    std::cout << "95th Percentile:" << p95 << " ns (" << p95 / 1000.0 << " us)\n";
    std::cout << "99th Percentile:" << p99 << " ns (" << p99 / 1000.0 << " us)\n";
    std::cout << "Max Latency:    " << maxLat << " ns (" << maxLat / 1000.0 << " us)\n\n";

    // "Danger window" is typically anything > 100-200 microseconds (0.1 - 0.2ms) since games might immediately draw after unmapping.
    // If we can do this under 50us, we are very safe.
    if (p99 < 50000) {
        std::cout << "[PASS] Overwrite occurs well within the danger window (before next draw call).\n";
    } else {
        std::cout << "[WARN] 99th percentile latency approaches danger window threshold!\n";
    }

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "Unknown exception!\n";
        return 1;
    }
    return 0;
}
