module;

#include "kiln/util/memory/lifetimebound.hpp"

export module kiln.gfx.renderer.command.TransferQueueRef;

import vulkan;

import kiln.gfx.renderer.command.Queue;
import kiln.gfx.renderer.command.QueueRefBase;
import kiln.gfx.renderer.command.TransferCommandBuffer;
import kiln.gfx.renderer.command.SubmitInfo;

namespace kiln::gfx::renderer {

class TransferQueueRefPrecondition {
public:
    explicit TransferQueueRefPrecondition(const Queue& queue);
};

export class TransferQueueRef : private TransferQueueRefPrecondition,
                                public QueueRefBase {
public:
    explicit TransferQueueRef(kiln_lifetimebound Queue& queue);

    auto submit(
        const TransferCommandBuffer& command_buffer,
        const SubmitInfo&            info = SubmitInfo{}
    ) const -> void;
};

}   // namespace kiln::gfx::renderer
