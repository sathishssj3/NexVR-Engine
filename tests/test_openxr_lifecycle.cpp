#include <gtest/gtest.h>
#include "../src/openxr/openxr_runtime_manager.h"
#include "../src/openxr/openxr_health_monitor.h"

using namespace vrinject::openxr;

TEST(OpenXRLifecycleTest, InitializationAndShutdown) {
    OpenXRHealthMonitor healthMonitor;
    OpenXRRuntimeManager manager(&healthMonitor);

    EXPECT_EQ(manager.GetState(), RuntimeState::UNINITIALIZED);

    // Note: Since this runs in a headless CI environment, Initialize might fail to get a system if no headset is attached
    // or if the mock isn't fully configured. We just test the initial call doesn't crash.
    // If we have a mock runtime, it will succeed.
    bool success = manager.Initialize("NexVR Engine Test", vrinject::GraphicsBackend::DX11);
    
    if (success) {
        EXPECT_EQ(manager.GetState(), RuntimeState::SYSTEM_SELECTED);
    } else {
        EXPECT_EQ(manager.GetState(), RuntimeState::FAILED);
    }

    manager.Shutdown();
    EXPECT_EQ(manager.GetState(), RuntimeState::UNINITIALIZED);
}

// More extensive testing would require injecting a mock OpenXR API Layer to push specific events.
