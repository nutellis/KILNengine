module;

#include "kiln/util/contract_macros.hpp"

module kiln.gfx.renderer.command.TransferQueueRef;

import vulkan;

import kiln.util.contracts;

namespace kiln::gfx::renderer {

TransferQueueRefPrecondition::TransferQueueRefPrecondition(
    [[maybe_unused]] const Queue& queue
)
{
    PRECOND(queue.flags() & vk::QueueFlagBits::eTransfer);
}

TransferQueueRef::TransferQueueRef(Queue& queue)
    : TransferQueueRefPrecondition{ queue },
      QueueRefBase{ queue }
{
}

auto TransferQueueRef::submit(
    const TransferCommandBuffer& command_buffer,
    const SubmitInfo&            info
) const -> void
{
    QueueRefBase::submit(command_buffer, info);
}

}   // namespace kiln::gfx::renderer
