#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <memory>
#include <iostream>
#include <thread>

#include "src/ai/ai_scheduler.h"
#include "src/ai/backend/vulkan_ai_backend.h"
#include "src/ai/backend/dx12_ai_backend.h"
#include "src/ai/backend/dx11_ai_backend.h"
#include "src/ai/backend_profiler.h"

using namespace vrinject::ai;

std::vector<std::unique_ptr<IAIBackend>> CreateAllBackends() {
    std::vector<std::unique_ptr<IAIBackend>> backends;
    backends.push_back(std::make_unique<VulkanAIBackend>());
    backends.push_back(std::make_unique<DX12AIBackend>());
    backends.push_back(std::make_unique<DX11AIBackend>());
    return backends;
}

TEST(CrossBackendAI, TestA_ZeroBlockingDelay) {
    auto backends = CreateAllBackends();
    
    for (auto& backend : backends) {
        std::string name = backend->GetName();
        std::cout << "Running Test A on " << name << "...\n";
        
        AIScheduler scheduler(std::move(backend));
        
        auto start = std::chrono::high_resolution_clock::now();
        // Submit 10 rapid frames. The backend simulates 1.1ms sleep per frame.
        for (int i = 1; i <= 10; ++i) {
            scheduler.PushJob(i);
        }
        auto end = std::chrono::high_resolution_clock::now();
        
        double pushTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
        
        // PushJob should be virtually instantaneous (queue is max 2)
        EXPECT_LT(pushTimeMs, 1.0) << name << " backend blocked the render thread!";
        
        // Verify graceful degradation is immediate
        EXPECT_FALSE(scheduler.IsJobReady(10)) << name << " backend returned true before work was actually done!";
    }
}

TEST(CrossBackendAI, TestB_QueueOverflowDropOldest) {
    auto backends = CreateAllBackends();
    
    for (auto& backend : backends) {
        std::string name = backend->GetName();
        std::cout << "Running Test B on " << name << "...\n";
        
        AIScheduler scheduler(std::move(backend));
        
        // Flood the queue
        for (int i = 1; i <= 100; ++i) {
            scheduler.PushJob(i);
        }
        
        // Wait for it to catch up
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        EXPECT_TRUE(scheduler.IsJobReady(100)) << name << " failed drop-oldest recovery!";
    }
}
