#include <atomic>
#include <chrono>
#include <print>
#include <thread>

import kiln.app.App;
import kiln.app.create;
import kiln.gfx.renderer.device.Device;
import kiln.gfx.vulkan.DebugMessenger;
import kiln.wsi.Context;
import kiln.wsi.event.Key;
import kiln.wsi.event.wait_events;

import examples.hello_triangle;

auto run(kiln::app::App& app) -> void;

auto main() -> int
{
    kiln::app::App app = kiln::app::create("Hello triangle!")
                             .register_entry<kiln::gfx::vulkan::DebugMessenger>()
                             .register_entry<demo::Context>()
                             .build();

    std::println(
        "Created renderer using {}",
        app.registry().at<kiln::gfx::renderer::Device>().name()
    );

    run(app);
}

auto run(kiln::app::App& app) -> void
{
    using namespace std::chrono_literals;

    const kiln::wsi::Context& wsi_context{ app.registry().at<kiln::wsi::Context>() };
    demo::Context&            demo_context{ app.registry().at<demo::Context>() };
    std::atomic_bool          running{ true };
    std::atomic_bool          window_resized{ false };
    std::atomic window_resolution{ demo_context.window().framebuffer_size() };

    std::jthread render_thread{
        [&demo_context, &running, &window_resized, &window_resolution] mutable -> void
        {
            auto last_time{ std::chrono::steady_clock::now() };
            while (running)
            {
                const auto now{ std::chrono::steady_clock::now() };
                const auto delta_time{ now - last_time };


                if (window_resized.exchange(false))
                {
                    demo_context.on_window_resize(window_resolution);
                }

                demo_context.render();


                std::this_thread::sleep_for(1s / 60 - delta_time);
                last_time = now;
            }
        }
    };

    while (!demo_context.window().should_close())
    {
        kiln::wsi::wait_events(wsi_context);

        if (demo_context.window().key_pressed(kiln::wsi::Key::eEscape))
        {
            demo_context.window().request_close();
        }

        // TODO: only do this on a window resize event
        window_resolution = demo_context.window().framebuffer_size();
        window_resized    = true;
    }

    running = false;
    render_thread.join();

    demo_context.shut_down();
}
