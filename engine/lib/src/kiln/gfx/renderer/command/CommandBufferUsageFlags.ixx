module;

#include <cstdint>

export module kiln.gfx.renderer.command.CommandBufferUsageFlags;

import kiln.util.EnumMask;

namespace kiln::gfx::renderer {

export enum struct CommandBufferUsageFlags : uint8_t {
    eNone     = 0,
    eReusable = 1,
};

}   // namespace kiln::gfx::renderer

template <>
constexpr bool kiln::util::enable_enum_mask<kiln::gfx::renderer::CommandBufferUsageFlags>
    = true;
