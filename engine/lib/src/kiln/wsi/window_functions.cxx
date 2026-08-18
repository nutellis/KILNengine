module;

#include <cassert>
#include <expected>
#include <optional>
#include <variant>

#include <GLFW/glfw3.h>

#include "kiln/util/contract_macros.hpp"

module kiln.wsi.window_functions;

import vulkan;

import kiln.util.contracts;
import kiln.util.Lazy;
import kiln.util.Overloaded;
import kiln.wsi.FullScreenWindowSettings;
import kiln.wsi.monitor_functions;
import kiln.wsi.Size;
import kiln.wsi.VideoMode;
import kiln.wsi.WindowedWindowSettings;
import kiln.wsi.WindowHandle;

namespace kiln::wsi {

auto create_window(const Context&, const char* const title, const WindowSettings& settings)
    -> WindowHandle
{
    PRECOND(title != nullptr);

    const auto [width, height]{
        std::visit(
            util::Overloaded{
                [](const WindowedWindowSettings& windowed_settings) static -> Size2u
                {
                    return windowed_settings.content_size;   //
                },
                [](const FullScreenWindowSettings& full_screen_settings) static -> Size2u
                {
                    return full_screen_settings.resolution.value_or(
                        util::Lazy{
                            [&] -> Size2u
                            {
                                return video_mode_of(full_screen_settings.monitor)
                                    .resolution;
                            },
                        }
                    );
                },
            },
            settings
        ),
    };

    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    if (const WindowedWindowSettings* windowed_settings{
            std::get_if<WindowedWindowSettings>(&settings) };
        windowed_settings != nullptr)
    {
        glfwWindowHint(
            GLFW_POSITION_X,
            windowed_settings->position_x.value_or(GLFW_ANY_POSITION)
        );
        glfwWindowHint(
            GLFW_POSITION_Y,
            windowed_settings->position_y.value_or(GLFW_ANY_POSITION)
        );

        const std::optional<WindowedWindowSettings::Border> border{
            windowed_settings->border
        };
        glfwWindowHint(GLFW_DECORATED, border.has_value());
        if (border.has_value())
        {
            glfwWindowHint(GLFW_RESIZABLE, border->resizable);
        }

        switch (windowed_settings->visibility)
        {
            case WindowedWindowSettings::Visibility::eDefault:
                glfwWindowHint(GLFW_FOCUSED, GLFW_FALSE);
                glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
                break;
            case WindowedWindowSettings::Visibility::eHidden:
                glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
                glfwWindowHint(GLFW_FOCUSED, GLFW_FALSE);
                break;
            case WindowedWindowSettings::Visibility::eFocused:
                glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
                glfwWindowHint(GLFW_FOCUSED, GLFW_TRUE);
                break;
        }

        glfwWindowHint(GLFW_MAXIMIZED, windowed_settings->maximized);
        glfwWindowHint(GLFW_FOCUS_ON_SHOW, windowed_settings->focus_on_show);
        glfwWindowHint(GLFW_SCALE_TO_MONITOR, windowed_settings->scale_to_monitor);
    }
    if (const FullScreenWindowSettings* full_screen_settings{
            std::get_if<FullScreenWindowSettings>(&settings) };
        full_screen_settings != nullptr)
    {
        glfwWindowHint(GLFW_AUTO_ICONIFY, full_screen_settings->auto_iconify);
        glfwWindowHint(GLFW_CENTER_CURSOR, full_screen_settings->center_cursor);
        glfwWindowHint(
            GLFW_REFRESH_RATE,
            full_screen_settings->refresh_rate.value_or(GLFW_DONT_CARE)
        );
    }

    GLFWwindow* const window = glfwCreateWindow(
        static_cast<int>(width),
        static_cast<int>(height),
        title,
        std::visit(
            util::Overloaded{
                [](const WindowedWindowSettings&) static -> GLFWmonitor*
                {
                    return nullptr;   //
                },
                [](const FullScreenWindowSettings& full_screen_settings) static
                    -> GLFWmonitor*
                {
                    return full_screen_settings.monitor.get();   //
                },
            },
            settings
        ),
        nullptr
    );

    assert(
        window != nullptr
        && "The registered error handler callback should have handled this error"
    );

    return WindowHandle{ window };
}

auto destroy_window(const Context&, const WindowHandle window) -> void
{
    glfwDestroyWindow(window.get());
}

auto should_close(const Context&, const WindowHandle window) noexcept -> bool
{
    PRECOND(window != nullptr);
    return glfwWindowShouldClose(window.get()) == GLFW_TRUE;
}

auto set_should_close_flag(
    const Context&,
    const WindowHandle window,
    const bool         flag
) noexcept -> void
{
    PRECOND(window != nullptr);
    glfwSetWindowShouldClose(window.get(), flag);
}

auto content_size_of(const Context&, const WindowHandle window) -> Size2u
{
    PRECOND(window != nullptr);

    int width{};
    int height{};
    glfwGetWindowSize(window.get(), &width, &height);

    assert(width >= 0);
    assert(height >= 0);

    return Size2u{
        .width  = static_cast<uint32_t>(width),
        .height = static_cast<uint32_t>(height),
    };
}

auto framebuffer_size_of(const Context&, const WindowHandle window) -> Size2u
{
    PRECOND(window != nullptr);

    int width{};
    int height{};
    glfwGetFramebufferSize(window.get(), &width, &height);

    assert(width >= 0);
    assert(height >= 0);

    return Size2u{
        .width  = static_cast<uint32_t>(width),
        .height = static_cast<uint32_t>(height),
    };
}

auto get_cursor_position(const Context&, const WindowHandle window) -> Position2d
{
    PRECOND(window != nullptr);

    Position2d result{};
    glfwGetCursorPos(window.get(), &result.x, &result.y);

    return result;
}

auto is_key_pressed(const Context&, const WindowHandle window, const Key key) -> bool
{
    PRECOND(window != nullptr);
    return glfwGetKey(window.get(), std::to_underlying(key)) == GLFW_PRESS;
}

auto set_title(const Context&, const WindowHandle window, const char* const title) -> void
{
    PRECOND(window != nullptr);
    glfwSetWindowTitle(window.get(), title);
}

[[nodiscard]]
// ReSharper disable once CppNotAllPathsReturnValue
auto raw_cursor_mode_from(const CursorMode cursor_mode) -> int
{
    switch (cursor_mode)
    {
        case CursorMode::eNormal:      return GLFW_CURSOR_NORMAL;
        case CursorMode::eHidden:      return GLFW_CURSOR_HIDDEN;
        case CursorMode::eCaptured:    return GLFW_CURSOR_CAPTURED;
        case CursorMode::eDisabled:    return GLFW_CURSOR_DISABLED;
        case CursorMode::eDisabledRaw: return GLFW_CURSOR_DISABLED;
    }
}

auto set_cursor_mode(
    const Context&,
    const WindowHandle window,
    const CursorMode   cursor_mode
) -> void
{
    PRECOND(window != nullptr);
    glfwSetInputMode(window.get(), GLFW_CURSOR, raw_cursor_mode_from(cursor_mode));
    glfwSetInputMode(
        window.get(),
        GLFW_RAW_MOUSE_MOTION,
        cursor_mode == CursorMode::eDisabledRaw && glfwRawMouseMotionSupported()
    );
}

auto create_vulkan_surface(const WindowHandle window, const vk::raii::Instance& instance)
    -> std::expected<vk::raii::SurfaceKHR, vk::Result>
{
    PRECOND(window != nullptr);

    VkSurfaceKHR   surface{};
    const VkResult result
        = glfwCreateWindowSurface(*instance, window.get(), nullptr, &surface);

    if (result != VK_SUCCESS)
    {
        return std::expected<vk::raii::SurfaceKHR, vk::Result>{
            std::unexpect,
            vk::Result{ result },
        };
    }

    return std::expected<vk::raii::SurfaceKHR, vk::Result>{
        vk::raii::SurfaceKHR{ instance, surface }
    };
}

auto set_window_user_pointer(const Context&, const WindowHandle window, void* user_pointer)
    -> void
{
    PRECOND(window != nullptr);
    glfwSetWindowUserPointer(window.get(), user_pointer);
}

}   // namespace kiln::wsi
