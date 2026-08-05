module;

#include <expected>

export module kiln.wsi.window_functions;

import vulkan;

import kiln.wsi.Context;
import kiln.wsi.CursorMode;
import kiln.wsi.event.events;
import kiln.wsi.event.Key;
import kiln.wsi.event.KeyAction;
import kiln.wsi.Position;
import kiln.wsi.Size;
import kiln.wsi.WindowHandle;
import kiln.wsi.WindowSettings;

namespace kiln::wsi {

export [[nodiscard]]
auto create_window(const Context&, const char* title, const WindowSettings& settings)
    -> WindowHandle;
export auto destroy_window(const Context&, WindowHandle window) -> void;

export [[nodiscard]]
auto        should_close(const Context&, WindowHandle window) noexcept -> bool;
export auto set_should_close_flag(const Context&, WindowHandle window, bool flag) noexcept
    -> void;

export [[nodiscard]]
auto content_size_of(const Context&, WindowHandle window) -> Size2u;
export [[nodiscard]]
auto framebuffer_size_of(const Context&, WindowHandle window) -> Size2u;

export [[nodiscard]]
auto get_cursor_position(const Context&, WindowHandle window) -> Position2d;
export [[nodiscard]]
auto is_key_pressed(const Context&, WindowHandle window, Key key) -> bool;

export auto set_title(const Context&, WindowHandle window, const char* title) -> void;
export auto set_cursor_mode(const Context&, WindowHandle window, CursorMode cursor_mode)
    -> void;

/*
 * This function should also take a context,
 * but we don't expose it due to the ability to be called from multiple threads
 */
export [[nodiscard]]
auto create_vulkan_surface(WindowHandle window, const vk::raii::Instance& instance)
    -> std::expected<vk::raii::SurfaceKHR, vk::Result>;

export auto set_window_user_pointer(
    const Context&,
    WindowHandle window,
    void*        user_pointer
) -> void;

}   // namespace kiln::wsi
