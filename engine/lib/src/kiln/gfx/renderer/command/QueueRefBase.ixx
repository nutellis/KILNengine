module;

#include <cstdint>
#include <functional>

#include "kiln/util/memory/lifetimebound.hpp"

export module kiln.gfx.renderer.command.QueueRefBase;

import vulkan;

import kiln.gfx.renderer.command.CommandBufferBase;
import kiln.gfx.renderer.command.Queue;
import kiln.gfx.renderer.command.SubmitInfo;
import kiln.gfx.vulkan.QueueFamilyIndex;

namespace kiln::gfx::renderer {

export class QueueRefBase {
public:
    [[nodiscard]]
    auto family_index() const noexcept -> vulkan::QueueFamilyIndex;
    [[nodiscard]]
    auto flags() const noexcept -> vk::QueueFlags;
    [[nodiscard]]
    auto index() const noexcept -> uint32_t;
    [[nodiscard]]
    auto get() const noexcept -> Queue&;

protected:
    explicit QueueRefBase(kiln_lifetimebound Queue& queue);

    auto submit(const CommandBufferBase& command_buffer, const SubmitInfo& info) const
        -> void;

private:
    std::reference_wrapper<Queue> m_queue_ref;
};

}   // namespace kiln::gfx::renderer
