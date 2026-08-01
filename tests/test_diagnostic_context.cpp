#include "gtest/gtest.h"
#include "src/core/diagnostic_context.h"
#include <thread>
#include <vector>

using namespace vrinject;

class DiagnosticContextTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Stats are global and persistent, so we just capture baseline
        m_startStats = DiagnosticContext::Get().GetStats();
    }
    DiagnosticQueueStats m_startStats;
};

TEST_F(DiagnosticContextTest, ThreadSafeEventPosting) {
    const int num_threads = 10;
    const int events_per_thread = 100;
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([i, events_per_thread]() {
            for (int j = 0; j < events_per_thread; ++j) {
                DiagnosticContext::Get().PostEvent(DiagnosticLevel::Info, "TestSubsystem", "Message from thread");
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto endStats = DiagnosticContext::Get().GetStats();
    uint64_t newEnqueued = endStats.eventsEnqueued - m_startStats.eventsEnqueued;
    uint64_t newDropped = endStats.eventsDropped - m_startStats.eventsDropped;
    
    EXPECT_EQ(newEnqueued + newDropped, static_cast<uint64_t>(num_threads * events_per_thread));
}

TEST_F(DiagnosticContextTest, DispatchOverheadProfile) {
    const int num_events = 100000;
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_events; ++i) {
        DiagnosticContext::Get().PostEvent(DiagnosticLevel::Info, "PerfTest", "Benchmarking dispatch");
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    // We expect 100k events to take less than 50ms (0.5 microseconds per dispatch max)
    std::cout << "Dispatched 100,000 events in " << duration_ms << " ms" << std::endl;
    EXPECT_LT(duration_ms, 50);
}
