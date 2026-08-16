#include <gtest/gtest.h>
#include "ai/ai_scheduler.h"
#include "ai/backend/ai_backend.h"

using namespace vrinject;
using namespace vrinject::ai;

class MockAIBackend : public IAIBackend {
public:
    bool Initialize() override { return true; }
    void Shutdown() override {}
    bool CreateResources() override { return true; }
    void DestroyResources() override {}
    uint64_t SubmitInference(const AIJob& job) override { return job.frameId; }
    bool PollCompletion(uint64_t jobId) override { return true; }
    void Synchronize(uint64_t jobId) override {}
    MemoryUsage GetMemoryUsage() const override { return {}; }
    TelemetryData GetTelemetry() const override { return {}; }
    void* GetUIMask() override { return reinterpret_cast<void*>(0xDEADBEEF); }
    const char* GetName() const override { return "Mock"; }
};

TEST(AISchedulerTest, Lifecycle) {
    auto backend = std::make_unique<MockAIBackend>();
    AIScheduler scheduler(std::move(backend));
    
    AIJob job;
    job.frameId = 1;
    job.colorTarget = nullptr;
    job.depthTarget = nullptr;
    
    scheduler.PushJob(job);
    
    // Allow thread to process
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    EXPECT_TRUE(scheduler.IsJobReady(1));
    EXPECT_EQ(scheduler.GetLatestUIMask(), reinterpret_cast<void*>(0xDEADBEEF));
}
