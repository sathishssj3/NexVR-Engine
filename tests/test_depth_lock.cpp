#include <gtest/gtest.h>
#include "heuristics/depth_lock_manager.h"
#include "heuristics/depth_candidate_collector.h"
#include "core/subsystem_context.h"
#include "core/logger.h"

using namespace vrinject;

class TestLogger : public vrinject::ILogger {
public:
    void Init(const std::string& logPath) override {}
    void Log(vrinject::ILogger::Level level, const char* file, int line, const char* fmt, ...) override {}
    void Shutdown() override {}
};

class DepthLockTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::cout << "SetUp started\n";
        auto config = std::make_shared<ConfigManager>();
        std::cout << "Created config\n";
        auto logger = std::make_shared<TestLogger>();
        std::cout << "Created logger\n";
        SubsystemContext::Get().Initialize(logger, config);
        std::cout << "Initialized SubsystemContext\n";
    }
    
    void TearDown() override {
        SubsystemContext::Get().Shutdown();
    }
};

TEST_F(DepthLockTest, StateMachineWorks) {
    std::cout << "Starting test" << std::endl;
    DepthLockManager& manager = *SubsystemContext::Get().GetDepthLockManager();
    std::cout << "Got manager" << std::endl;
    manager.OnDeviceLost(); // Reset
    std::cout << "OnDeviceLost done" << std::endl;
    
    EXPECT_EQ(manager.GetState(), DepthLockState::LOST);
    
    // Simulate candidate
    void* dummyPtr = (void*)0x1234;
    SubsystemContext::Get().GetDepthCandidateCollector()->OnDepthSurfaceCreated(dummyPtr, 1920, 1080, 40, 1, 1, 1, 1); // DXGI_FORMAT_D32_FLOAT = 40
    SubsystemContext::Get().GetDepthCandidateCollector()->OnOMSetRenderTargets(dummyPtr);
    SubsystemContext::Get().GetDepthCandidateCollector()->OnClearDepthStencilView(dummyPtr, 1.0f);
    std::cout << "Collector setup done" << std::endl;
    
    for (int i = 0; i < 5; ++i) {
        std::cout << "Frame " << i << std::endl;
        manager.OnFrameEnd(i, 1, 1, 1920, 1080);
        if (i == 0) {
            EXPECT_EQ(manager.GetState(), DepthLockState::VERIFYING);
        }
    }
    
    EXPECT_EQ(manager.GetState(), DepthLockState::LOCKED);
    EXPECT_EQ(manager.GetSnapshot().convention, DepthConvention::Standard);
}
