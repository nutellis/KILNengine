module;

#include <format>
#include <stdexcept>

#include <fmt/compile.h>

#include "kiln/util/contract_macros.hpp"

module kiln.gfx.vulkan.result.VulkanError;

import vulkan;

import kiln.gfx.vulkan.format.to_string;
import kiln.gfx.vulkan.result.result_category_from;
import kiln.gfx.vulkan.result.result_description;
import kiln.gfx.vulkan.result.ResultCategory;
import kiln.util.contracts;

namespace kiln::gfx::vulkan {

VulkanErrorPrecondition::VulkanErrorPrecondition(
    [[maybe_unused]] const vk::Result runtime_error_code
)
{
    using namespace fmt::literals;

    PRECOND(
        result_category_from(runtime_error_code) == ResultCategory::eRuntimeError,
        fmt::format(
            "Error code (`{}`) does not represent a runtime error"_cf,
            vulkan::to_string(runtime_error_code)
        )
    );
}

VulkanError::VulkanError(const vk::Result runtime_error_code)
    : VulkanErrorPrecondition{ runtime_error_code },
      std::runtime_error{
          std::format(
              "`{}` - {}",
              vulkan::to_string(runtime_error_code),
              result_description(runtime_error_code).get()
          ),
      }
{
}

}   // namespace kiln::gfx::vulkan
