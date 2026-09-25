#pragma once

#include <vulkan/vulkan.h>

namespace vrinject {
namespace vulkan {
namespace hooks {
namespace detail {

// Never acknowledge a queue operation that was not forwarded to the driver.
template <typename Function, typename... Args>
VkResult ForwardQueueCall(Function direct, Function dispatch, Args... args) {
    const auto forward = direct ? direct : dispatch;
    return forward ? forward(args...) : VK_ERROR_INITIALIZATION_FAILED;
}

} // namespace detail
} // namespace hooks
} // namespace vulkan
} // namespace vrinject
