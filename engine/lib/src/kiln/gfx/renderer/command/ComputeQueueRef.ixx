module;

#include "kiln/util/memory/lifetimebound.hpp"

export module kiln.gfx.renderer.command.ComputeQueueRef;

import vulkan;

import kiln.gfx.renderer.command.ComputeCommandBuffer;
import kiln.gfx.renderer.command.Queue;
import kiln.gfx.renderer.command.SubmitInfo;
import kiln.gfx.renderer.command.TransferQueueRef;

namespace kiln::gfx::renderer {

class ComputeQueueRefPrecondition {
public:
    explicit ComputeQueueRefPrecondition(const Queue& queue);
};

export class ComputeQueueRef : private ComputeQueueRefPrecondition,
                               public TransferQueueRef {
public:
    explicit ComputeQueueRef(kiln_lifetimebound Queue& queue);

    auto submit(
        const ComputeCommandBuffer& command_buffer,
        const SubmitInfo&           info = SubmitInfo{}
    ) const -> void;
};

}   // namespace kiln::gfx::renderer
