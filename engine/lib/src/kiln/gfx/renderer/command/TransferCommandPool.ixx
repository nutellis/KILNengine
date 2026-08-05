module;

#include "kiln/util/memory/lifetimebound.hpp"

export module kiln.gfx.renderer.command.TransferCommandPool;

import vulkan;

import kiln.gfx.renderer.command.CommandBufferUsageFlags;
import kiln.gfx.renderer.command.CommandPoolBase;
import kiln.gfx.renderer.command.CommandPoolFlags;
import kiln.gfx.renderer.command.TransferCommandBuffer;
import kiln.gfx.renderer.device.Device;
import kiln.gfx.vulkan.QueueFamilyIndex;
import kiln.util.EnumMask;

namespace kiln::gfx::renderer {

export class TransferCommandPool : public CommandPoolBase {
public:
    // clang-format off
    explicit TransferCommandPool(
        kiln_lifetimebound const Device& device,
        vulkan::QueueFamilyIndex         queue_family_index,
        util::EnumMask<CommandPoolFlags> flags = CommandPoolFlags::eNone
    );
    // clang-format on

    auto allocate_primary(
        util::EnumMask<CommandBufferUsageFlags> usage_flags
        = CommandBufferUsageFlags::eNone
    ) -> TransferCommandBuffer;
};

}   // namespace kiln::gfx::renderer

module :private;

namespace kiln::gfx::renderer {

TransferCommandPool::TransferCommandPool(
    const Device&                          device,
    const vulkan::QueueFamilyIndex         queue_family_index,
    const util::EnumMask<CommandPoolFlags> flags
)
    : CommandPoolBase{ device, queue_family_index, flags }
{
}

auto TransferCommandPool::allocate_primary(
    const util::EnumMask<CommandBufferUsageFlags> usage_flags
) -> TransferCommandBuffer
{
    return TransferCommandBuffer{ CommandPoolBase::allocate_primary(), usage_flags };
}

}   // namespace kiln::gfx::renderer
