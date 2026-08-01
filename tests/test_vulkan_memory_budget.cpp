#include <gtest/gtest.h>

#include "src/rendering/vulkan_memory_budget.h"

using namespace vrinject::vulkan;

TEST(VulkanMemoryBudget, TracksResourceAndDeviceBudgets) {
    VulkanMemoryBudget budget;
    budget.SetExtensionAvailable(true);
    budget.UpdateTrackedResources(100, 20, 5, 3);
    budget.UpdateDeviceBudget(1000, 250);

    auto snapshot = budget.GetSnapshot();
    EXPECT_TRUE(snapshot.extMemoryBudgetAvailable);
    EXPECT_EQ(snapshot.totalTrackedBytes, 128u);
    EXPECT_EQ(snapshot.budgetBytes, 1000u);
    EXPECT_EQ(snapshot.usageBytes, 250u);
}
