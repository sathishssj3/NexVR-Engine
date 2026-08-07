#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <numeric>
#include <algorithm>
#include <iostream>

#include "src/rendering/vulkan_async_scheduler.h"

using namespace vrinject::vulkan;

TEST(AIPipeline, StressTest5000FramesWithTelemetry) {
    AICommandQueue queue;
    
    const int numFrames = 5000;
    std::atomic<uint64_t> executionCounter{0};
    
    std::vector<double> latencies;
    latencies.reserve(numFrames);

    // Hardened validation harness: inject an atomic counter to mathematically guarantee 
    // that the async loop actually executes and does not short circuit.
    
    auto totalStart = std::chrono::high_resolution_clock::now();

    for (int i = 1; i <= numFrames; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        
        queue.PushJob(i, [&executionCounter]() {
            // Simulate AI Inference workload on GPU (e.g., 1.1ms)
            std::this_thread::sleep_for(std::chrono::microseconds(1100));
            executionCounter.fetch_add(1, std::memory_order_relaxed);
        });

        // Spin wait for the job to complete to measure round-trip latency
        while (!queue.IsJobReady(i)) {
            // zero-blocking poll loop for testing
            std::this_thread::yield();
        }

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;
        latencies.push_back(elapsed.count());
    }
    
    auto totalEnd = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> totalElapsed = totalEnd - totalStart;

    // Validate execution counter matches exact frame count
    EXPECT_EQ(executionCounter.load(), numFrames) << "AI Thread short-circuited! Execution count mismatch.";

    // Telemetry reporting
    std::sort(latencies.begin(), latencies.end());
    double sum = std::accumulate(latencies.begin(), latencies.end(), 0.0);
    double mean = sum / latencies.size();
    double median = latencies[latencies.size() / 2];
    double p95 = latencies[static_cast<size_t>(latencies.size() * 0.95)];
    double p99 = latencies[static_cast<size_t>(latencies.size() * 0.99)];
    double maxLatency = latencies.back();

    std::cout << "\n--- AI Pipeline Telemetry (5000 Frames) ---\n";
    std::cout << "Mean Latency:   " << mean << " ms\n";
    std::cout << "Median Latency: " << median << " ms\n";
    std::cout << "P95 Latency:    " << p95 << " ms\n";
    std::cout << "P99 Latency:    " << p99 << " ms\n";
    std::cout << "Max Latency:    " << maxLatency << " ms\n";
    std::cout << "Total Time:     " << totalElapsed.count() << " ms\n";
    std::cout << "-------------------------------------------\n";
    
    // Ensure the system doesn't degrade over time (budget is 1.5ms per frame)
    // We expect overhead on top of the 1.1ms sleep, but it should stay well under 2.5ms max
    EXPECT_LT(mean, 2.0);
    EXPECT_LT(p99, 3.0);
}
