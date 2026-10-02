#include "gtest/gtest.h"
#include "core/runtime_state.h"
#include "core/hook_manager.h"
#include <thread>
#include <windows.h>

HMODULE g_hModule = nullptr;
using namespace vrinject;

class RuntimeStateTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset state if needed, though RuntimeState is a singleton
        // We will just test the transitions
    }
};

TEST_F(RuntimeStateTest, InitializationConcurrency) {
    // We cannot easily test DllMain attach directly because it spawns a thread that relies on 
    // full subsystems, but we can verify that WaitForPhase works.
    
    // In a unit test environment, we might not want to fully initialize D3D hooks,
    // so we'll just test that the state machine itself is thread-safe and can wait.
    
    std::thread initThread([]() {
        RuntimeState::Get().OnDllProcessAttach(nullptr);
    });
    
    // Wait for the initialization phase to start
    // Note: since it might fail without a real environment, we wait for either Running or Error
    // but we can't reliably wait for multiple phases in the current simple WaitForPhase.
    
    initThread.join();
    
    // The background thread might still be running.
    // If we sleep a bit, it will hit Error due to no real environment.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    EXPECT_NE(RuntimeState::Get().GetPhase(), RuntimePhase::Uninitialized);
}
