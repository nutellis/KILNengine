module;

#include <cassert>
#include <expected>
#include <span>

#include <GLFW/glfw3.h>

#include "kiln/util/memory/lifetimebound.hpp"

export module kiln.wsi.vulkan_instance_extensions;

import kiln.wsi.Context;

namespace kiln::wsi {

export struct VulkanSurfaceCreationNotSupportedError {};

// clang-format off
export [[nodiscard]]
auto vulkan_instance_extensions(kiln_lifetimebound const Context&)
    -> std::expected<std::span<const char* const>, VulkanSurfaceCreationNotSupportedError>
// clang-format on
{
    uint32_t           count{};
    const char** const extension_names{ glfwGetRequiredInstanceExtensions(&count) };

    assert(
        extension_names != nullptr
        && "The registered error handler callback should have handled this error"
    );

    return std::span{
        extension_names,
        count,
    };
}

}   // namespace kiln::wsi
