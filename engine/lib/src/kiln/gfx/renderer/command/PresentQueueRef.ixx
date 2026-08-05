module;

#include <variant>

#include "kiln/util/memory/lifetimebound.hpp"

export module kiln.gfx.renderer.command.PresentQueueRef;

import vulkan;

import kiln.gfx.renderer.command.Queue;
import kiln.gfx.renderer.command.QueueRefBase;
import kiln.gfx.vulkan.result.TypedResultCode;

namespace kiln::gfx::renderer {

class PresentQueueRefPrecondition {
public:
    explicit PresentQueueRefPrecondition(const Queue& queue);
};

export class PresentQueueRef : private PresentQueueRefPrecondition, public QueueRefBase {
public:
    explicit PresentQueueRef(kiln_lifetimebound Queue& queue);

    auto present(const vk::PresentInfoKHR& present_info) const -> std::variant<
        vulkan::TypedResultCode<vk::Result::eSuccess>,
        vulkan::TypedResultCode<vk::Result::eSuboptimalKHR>,
        vulkan::TypedResultCode<vk::Result::eErrorOutOfDateKHR>>;
};

}   // namespace kiln::gfx::renderer
