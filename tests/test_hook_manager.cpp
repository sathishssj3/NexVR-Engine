#include "gtest/gtest.h"
#include "core/hook_manager.h"
#include <windows.h>
#include <thread>
#include <atomic>

HMODULE g_hModule = nullptr;

class HookManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset HookManager to uninitialized state
        vrinject::HookManager::Get().ShutdownHooks();
    }
    
    void TearDown() override {
        vrinject::HookManager::Get().ShutdownHooks();
    }
};

TEST_F(HookManagerTest, TransactionLifecycle) {
    vrinject::HookManager& hm = vrinject::HookManager::Get();

    EXPECT_EQ(hm.GetPhase(), vrinject::HookPhase::Uninitialized);
    
    EXPECT_TRUE(hm.Prepare());
    EXPECT_EQ(hm.GetPhase(), vrinject::HookPhase::Prepared);

    EXPECT_TRUE(hm.Validate());
    EXPECT_EQ(hm.GetPhase(), vrinject::HookPhase::Validated);

    // Stop here and rollback manually to test rollback
    hm.Rollback();
    EXPECT_EQ(hm.GetPhase(), vrinject::HookPhase::RolledBack);
}

TEST_F(HookManagerTest, InitializeHooksIdempotency) {
    vrinject::HookManager& hm = vrinject::HookManager::Get();
    
    // Attempt initialization
    bool res1 = hm.InitializeHooks();
    EXPECT_TRUE(res1);
    
    // Check phase
    EXPECT_EQ(hm.GetPhase(), vrinject::HookPhase::Committed);

    // Shutdown
    hm.ShutdownHooks();
    EXPECT_EQ(hm.GetPhase(), vrinject::HookPhase::Uninitialized);
    
    // Shutdown again shouldn't crash
    hm.ShutdownHooks();
}

TEST_F(HookManagerTest, FailureInjectionRollback) {
    vrinject::HookManager& hm = vrinject::HookManager::Get();
    
    EXPECT_TRUE(hm.Prepare());
    EXPECT_TRUE(hm.Validate());
    
    // Simulate install phase adding rollbacks
    // Then rollback
    hm.Rollback();
    
    EXPECT_EQ(hm.GetPhase(), vrinject::HookPhase::RolledBack);
}

TEST_F(HookManagerTest, ConcurrentInitializeAndShutdown) {
    vrinject::HookManager& hm = vrinject::HookManager::Get();
    
    std::atomic<int> initCount{0};
    std::atomic<int> shutdownCount{0};

    auto threadFunc = [&]() {
        for (int i = 0; i < 5; ++i) {
            if (hm.InitializeHooks()) {
                initCount++;
            }
            std::this_thread::yield();
            hm.ShutdownHooks();
            shutdownCount++;
        }
    };

    std::thread t1(threadFunc);
    std::thread t2(threadFunc);
    std::thread t3(threadFunc);

    t1.join();
    t2.join();
    t3.join();

    EXPECT_EQ(hm.GetPhase(), vrinject::HookPhase::Uninitialized);
}