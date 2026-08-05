module;

#include <functional>

#include "kiln/util/memory/lifetimebound.hpp"

export module kiln.wsi.Engine;

import kiln.wsi.Context;
import kiln.wsi.EventConsumerQueueInterface;
import kiln.wsi.WindowHandle;
import kiln.wsi.WindowSettings;

namespace kiln::wsi {

class EnginePrecondition {
public:
    EnginePrecondition();
    ~EnginePrecondition();

    static uint32_t instance_count;
};

export class Engine : EnginePrecondition {
public:
    explicit Engine(kiln_lifetimebound EventConsumerQueueInterface& event_consume_queue);


    [[nodiscard]]
    auto context() const noexcept -> const Context&;


    [[nodiscard]]
    auto create_window(const char* title, const WindowSettings& settings) const
        -> WindowHandle;

    auto poll_events() const -> void;
    auto wait_events() const -> void;

    auto post_empty_event() const -> void;

private:
    Context                                             m_context;
    std::reference_wrapper<EventConsumerQueueInterface> m_event_consume_queue_ref;

    static auto set_callbacks(WindowHandle window) -> void;
};

}   // namespace kiln::wsi
