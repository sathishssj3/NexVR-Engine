#include <gtest/gtest.h>
#include "rendering/asw_manager.h"
#include <thread>
#include <chrono>

using namespace vrinject;

TEST(AswManagerTest, ShouldSynthesizeFrame_ZeroTarget) {
    AswManager asw;
    EXPECT_FALSE(asw.ShouldSynthesizeFrame(0));
}

TEST(AswManagerTest, SynthesizeWhenFallingBehind) {
    AswManager asw;
    // Initial state: just submitted a frame
    asw.OnRealFrameSubmitted();
    
    // Simulate game missing 11.1ms deadline (90Hz)
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
    
    // Should now require a synthetic frame
    EXPECT_TRUE(asw.ShouldSynthesizeFrame(90));
}

TEST(AswManagerTest, NoSynthesizeWhenOnTime) {
    AswManager asw;
    asw.OnRealFrameSubmitted();
    
    // Extremely fast frame
    EXPECT_FALSE(asw.ShouldSynthesizeFrame(90));
}
