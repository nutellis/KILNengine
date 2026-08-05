module;

#include "kiln/util/memory/lifetimebound.hpp"

export module kiln.gfx.renderer.command.QueueRef;

import kiln.gfx.renderer.command.CommandBufferBase;
import kiln.gfx.renderer.command.ComputeQueueRef;
import kiln.gfx.renderer.command.GraphicsQueueRef;
import kiln.gfx.renderer.command.PresentQueueRef;
import kiln.gfx.renderer.command.Queue;
import kiln.gfx.renderer.command.QueueRefBase;
import kiln.gfx.renderer.command.SubmitInfo;
import kiln.gfx.renderer.command.TransferQueueRef;

namespace kiln::gfx::renderer {

export class QueueRef : public QueueRefBase {
public:
    explicit QueueRef(kiln_lifetimebound Queue& queue);

    [[nodiscard]]
    auto as_compute_queue() const -> ComputeQueueRef;
    [[nodiscard]]
    auto as_graphics_queue() const -> GraphicsQueueRef;
    [[nodiscard]]
    auto as_present_queue() const -> PresentQueueRef;
    [[nodiscard]]
    auto as_transfer_queue() const -> TransferQueueRef;

    auto submit(const CommandBufferBase& command_buffer, const SubmitInfo& info) const
        -> void;
};

}   // namespace kiln::gfx::renderer
