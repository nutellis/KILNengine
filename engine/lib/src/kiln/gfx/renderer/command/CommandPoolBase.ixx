module;

#include <functional>

#include "kiln/util/memory/lifetimebound.hpp"

export module kiln.gfx.renderer.command.CommandPoolBase;

import vulkan;

import kiln.gfx.renderer.command.CommandPoolFlags;
import kiln.gfx.renderer.device.Device;
import kiln.gfx.vulkan.QueueFamilyIndex;
import kiln.util.EnumMask;

namespace kiln::gfx::renderer {

export class CommandPoolBase {
public:
    // clang-format off
    explicit CommandPoolBase(
        kiln_lifetimebound const Device& device,
        vulkan::QueueFamilyIndex         queue_family_index,
        util::EnumMask<CommandPoolFlags> flags = CommandPoolFlags::eNone
    );
    // clang-format on


    [[nodiscard]]
    auto queue_family_index() const noexcept -> vulkan::QueueFamilyIndex;

    auto reset() -> void;

protected:
    auto allocate_primary() kiln_lifetimebound -> vk::raii::CommandBuffer;

private:
    std::reference_wrapper<const Device> m_device;
    vulkan::QueueFamilyIndex             m_queue_family_index;
    vk::raii::CommandPool                m_command_pool;
};

}   // namespace kiln::gfx::renderer
