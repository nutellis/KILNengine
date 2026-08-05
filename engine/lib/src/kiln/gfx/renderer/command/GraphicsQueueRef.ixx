module;

#include "kiln/util/memory/lifetimebound.hpp"

export module kiln.gfx.renderer.command.GraphicsQueueRef;

import vulkan;

import kiln.gfx.renderer.command.GraphicsCommandBuffer;
import kiln.gfx.renderer.command.Queue;
import kiln.gfx.renderer.command.SubmitInfo;
import kiln.gfx.renderer.command.TransferQueueRef;

namespace kiln::gfx::renderer {

class GraphicsQueueRefPrecondition {
public:
    explicit GraphicsQueueRefPrecondition(const Queue& queue);
};

export class GraphicsQueueRef : private GraphicsQueueRefPrecondition,
                                public TransferQueueRef {
public:
    explicit GraphicsQueueRef(kiln_lifetimebound Queue& queue);

    auto submit(
        const GraphicsCommandBuffer& command_buffer,
        const SubmitInfo&            info = SubmitInfo{}
    ) const -> void;
};

}   // namespace kiln::gfx::renderer
