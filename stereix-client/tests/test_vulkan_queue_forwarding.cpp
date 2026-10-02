#include <gtest/gtest.h>
#include "hooks/vulkan_queue_forwarding.h"

using vrinject::vulkan::hooks::detail::ForwardQueueCall;

namespace {
int directSubmitCalls = 0;
int dispatchSubmitCalls = 0;
int directPresentCalls = 0;
int dispatchPresentCalls = 0;

VkResult VKAPI_CALL DirectSubmit(VkQueue, uint32_t, const VkSubmitInfo*, VkFence) {
    ++directSubmitCalls;
    return VK_ERROR_DEVICE_LOST;
}

VkResult VKAPI_CALL DispatchSubmit(VkQueue, uint32_t, const VkSubmitInfo*, VkFence) {
    ++dispatchSubmitCalls;
    return VK_SUCCESS;
}

VkResult VKAPI_CALL DirectPresent(VkQueue, const VkPresentInfoKHR*) {
    ++directPresentCalls;
    return VK_SUBOPTIMAL_KHR;
}

VkResult VKAPI_CALL DispatchPresent(VkQueue, const VkPresentInfoKHR*) {
    ++dispatchPresentCalls;
    return VK_ERROR_OUT_OF_DATE_KHR;
}
} // namespace

TEST(VulkanQueueForwardingTest, SubmitPrefersDirectAndPropagatesDriverResult) {
    directSubmitCalls = 0;
    dispatchSubmitCalls = 0;
    PFN_vkQueueSubmit direct = DirectSubmit;
    PFN_vkQueueSubmit dispatch = DispatchSubmit;

    EXPECT_EQ(ForwardQueueCall(direct, dispatch, VK_NULL_HANDLE, 0u, static_cast<const VkSubmitInfo*>(nullptr), VK_NULL_HANDLE),
              VK_ERROR_DEVICE_LOST);
    EXPECT_EQ(directSubmitCalls, 1);
    EXPECT_EQ(dispatchSubmitCalls, 0);
    EXPECT_EQ(ForwardQueueCall(static_cast<PFN_vkQueueSubmit>(nullptr), dispatch, VK_NULL_HANDLE, 0u,
                               static_cast<const VkSubmitInfo*>(nullptr), VK_NULL_HANDLE), VK_SUCCESS);
    EXPECT_EQ(dispatchSubmitCalls, 1);
    EXPECT_EQ(ForwardQueueCall(static_cast<PFN_vkQueueSubmit>(nullptr), static_cast<PFN_vkQueueSubmit>(nullptr),
                               VK_NULL_HANDLE, 0u, static_cast<const VkSubmitInfo*>(nullptr), VK_NULL_HANDLE),
              VK_ERROR_INITIALIZATION_FAILED);
}

TEST(VulkanQueueForwardingTest, PresentPrefersDirectAndPropagatesDriverResult) {
    directPresentCalls = 0;
    dispatchPresentCalls = 0;
    PFN_vkQueuePresentKHR direct = DirectPresent;
    PFN_vkQueuePresentKHR dispatch = DispatchPresent;

    EXPECT_EQ(ForwardQueueCall(direct, dispatch, VK_NULL_HANDLE, static_cast<const VkPresentInfoKHR*>(nullptr)),
              VK_SUBOPTIMAL_KHR);
    EXPECT_EQ(directPresentCalls, 1);
    EXPECT_EQ(dispatchPresentCalls, 0);
    EXPECT_EQ(ForwardQueueCall(static_cast<PFN_vkQueuePresentKHR>(nullptr), dispatch, VK_NULL_HANDLE,
                               static_cast<const VkPresentInfoKHR*>(nullptr)), VK_ERROR_OUT_OF_DATE_KHR);
    EXPECT_EQ(dispatchPresentCalls, 1);
    EXPECT_EQ(ForwardQueueCall(static_cast<PFN_vkQueuePresentKHR>(nullptr), static_cast<PFN_vkQueuePresentKHR>(nullptr),
                               VK_NULL_HANDLE, static_cast<const VkPresentInfoKHR*>(nullptr)),
              VK_ERROR_INITIALIZATION_FAILED);
}
