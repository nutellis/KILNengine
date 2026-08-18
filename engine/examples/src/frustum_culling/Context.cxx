module;

#include <atomic>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <future>
#include <memory_resource>
#include <optional>
#include <print>
#include <ranges>
#include <thread>

#include <kiln/util/contract_macros.hpp>

module examples.frustum_culling.Context;

import vulkan;

import kiln.app.memory.MemoryArena;
import kiln.event.Timestamp;
import kiln.gfx.asset.gltf.Parser;
import kiln.gfx.renderer.command.Queue;
import kiln.gfx.renderer.command.QueueProvider;
import kiln.gfx.renderer.command.QueueProviderBuilder;
import kiln.gfx.renderer.command.QueueType;
import kiln.gfx.renderer.device.DeviceBuilder;
import kiln.gfx.renderer.pipeline.PipelineContext;
import kiln.gfx.renderer.presentation.PresentationContext;
import kiln.gfx.vulkan.Instance;
import kiln.gfx.vulkan.InstanceBuilder;
import kiln.gfx.vulkan.result.check_result;
import kiln.reg.BuildableEntryBuilder;
import kiln.reg.BuildDirector;
import kiln.util.containers.MoveOnlyFunction;
import kiln.util.contracts;
import kiln.util.Lazy;
import kiln.util.ScopeFail;
import kiln.wsi.Context;
import kiln.wsi.CursorMode;
import kiln.wsi.Engine;
import kiln.wsi.event.Event;
import kiln.wsi.event.events;
import kiln.wsi.event.EventType;
import kiln.wsi.event.Key;
import kiln.wsi.EventConsumerQueueInterface;
import kiln.wsi.window_functions;
import kiln.wsi.WindowCommand;
import kiln.wsi.WindowedWindowSettings;
import kiln.wsi.WindowHandle;
import kiln.wsi.WindowProxy;

import examples.frustum_culling.AABB;
import examples.frustum_culling.Camera;
import examples.frustum_culling.Controller;
import examples.frustum_culling.gltf_utils;
import examples.frustum_culling.load_scene;
import examples.frustum_culling.FixedSPSCQueue;
import examples.frustum_culling.workflow.ModelDescription;
import examples.frustum_culling.workflow.Renderer;
import examples.frustum_culling.workflow.Scene;

namespace demo {

class ContextBuilder;

auto describe_injection(kiln::reg::BuildDirector<ContextBuilder>& build_director) -> void;

class ContextBuilder
    : public kiln::reg::BuildableEntryBuilder<Context, ContextBuilder, describe_injection>   //
{
public:
    [[nodiscard]]
    // ReSharper disable once CppDeclaratorNeverUsed
    static auto create(
        kiln::gfx::vulkan::InstanceBuilder&        instance_builder,
        kiln::gfx::renderer::DeviceBuilder&        device_builder,
        kiln::gfx::renderer::QueueProviderBuilder& queue_provider_builder
    ) -> ContextBuilder
    {
        instance_builder.target_api_version(vk::ApiVersion14);
        device_builder.enable_features(
            vk::PhysicalDeviceFeatures{
                .multiDrawIndirect         = vk::True,
                .drawIndirectFirstInstance = vk::True,
                .shaderInt64               = vk::True,
            }
        );
        device_builder.enable_features(
            vk::PhysicalDeviceVulkan12Features{
                .drawIndirectCount    = vk::True,
                .storagePushConstant8 = vk::True,
                .shaderInt8           = vk::True,
                .scalarBlockLayout    = vk::True,
                .bufferDeviceAddress  = vk::True,
            }
        );
        device_builder.enable_features(
            vk::PhysicalDeviceVulkan13Features{ .maintenance4 = vk::True }
        );
        device_builder.enable_features(
            vk::PhysicalDeviceVulkan14Features{ .maintenance5 = vk::True }
        );

        device_builder.enable_extension(vk::KHRMaintenance9ExtensionName);
        device_builder.enable_features(
            vk::PhysicalDeviceMaintenance9FeaturesKHR{ .maintenance9 = vk::True }
        );

        queue_provider_builder.require_queue(kiln::gfx::renderer::QueueType::eGraphics);
        queue_provider_builder.require_queue(kiln::gfx::renderer::QueueType::eCompute);

        return ContextBuilder{};
    }

    [[nodiscard]]
    // ReSharper disable once CppDeclaratorNeverUsed
    static auto build(
        const kiln::app::Config&,
        kiln::app::MemoryArena& memory_arena,
        const kiln::gfx::vulkan::Instance&,
        const kiln::wsi::Context&,
        const kiln::gfx::renderer::Device&  render_device,
        kiln::gfx::renderer::QueueProvider& gpu_queue_provider,
        kiln::gfx::renderer::Allocator&,
        const kiln::gfx::renderer::PipelineContext&,
        const kiln::gfx::renderer::PresentationContext&,
        kiln::gfx::asset::gltf::Parser&
    ) -> Context
    {
        return Context{
            std::allocator_arg,
            memory_arena.pool_allocator(),
            render_device,
            gpu_queue_provider,
        };
    }
};

auto describe_injection(kiln::reg::BuildDirector<ContextBuilder>& build_director) -> void
{
    build_director.use_function<ContextBuilder::create>();
}

[[nodiscard]]
// ReSharper disable once CppNotAllPathsReturnValue
auto select_staging_queue(
    const kiln::gfx::renderer::Queue&,
    const kiln::gfx::renderer::QueueType queue_type
) -> std::optional<uint32_t>
{
    switch (queue_type)
    {
        case kiln::gfx::renderer::QueueType::eGraphics:             return 0;
        case kiln::gfx::renderer::QueueType::eCompute:              return 1;
        case kiln::gfx::renderer::QueueType::eHostToDeviceTransfer: return 2;
    }
}

Context::Context(Context&& other, const allocator_type& allocator)
    : m_wsi_event_buffer{ std::move(other.m_wsi_event_buffer), allocator },
      m_wsi_event_recorder{ std::move(other.m_wsi_event_recorder), allocator },
      m_staging_stream{ std::move(other.m_staging_stream), allocator }
{
}

Context::Context(
    const kiln::gfx::renderer::Device&  render_device,
    kiln::gfx::renderer::QueueProvider& render_queue_provider
)
    : Context{
          std::allocator_arg,
          std::pmr::get_default_resource(),
          render_device,
          render_queue_provider,
      }
{
}

Context::Context(
    std::allocator_arg_t,
    const allocator_type&               allocator,
    const kiln::gfx::renderer::Device&  render_device,
    kiln::gfx::renderer::QueueProvider& render_queue_provider
)
    : m_wsi_event_buffer{ std::allocator_arg, allocator },
      m_wsi_event_recorder{ std::allocator_arg, allocator },
      m_staging_stream{
          std::allocator_arg,
          allocator,
          render_device,
          render_queue_provider.select_queue(select_staging_queue)->as_transfer_queue(),
      }
{
}

class Context::EventQueue : public kiln::wsi::EventConsumerQueueInterface {
public:
    using value_type = std::pair<kiln::wsi::Event, kiln::event::Timestamp>;

    auto push(kiln::wsi::Event&& event, kiln::event::Timestamp timestamp) -> void override
    {
        [[maybe_unused]]
        const bool success = m_concurrent_queue.try_emplace(std::move(event), timestamp);
        assert(success);
    }

    template <typename F>
    auto pop_all(F&& callback) -> void
    {
        m_concurrent_queue.pop_all(
            [&callback](const auto popped_view) -> void
            {
                for (value_type&& value : popped_view)
                {
                    std::apply(callback, std::move(value));
                }
            }
        );
    }

private:
    FixedSPSCQueue<value_type> m_concurrent_queue{ max_enqueued_items };
};

struct Context::MainThread {
    using Task = kiln::util::MoveOnlyFunction<auto(MainThread&) &&->void>;

    EventQueue           event_queue;
    kiln::wsi::Engine    wsi_engine{ event_queue };
    FixedSPSCQueue<Task> work_queue{ max_enqueued_items };
};

auto Context::get_allocator() const noexcept -> allocator_type
{
    return m_wsi_event_buffer.get_allocator();
}

auto Context::run(
    kiln::app::App&              app,
    const std::filesystem::path& model_filepath,
    const bool                   limit_fps,
    const bool                   disable_culling,
    const uint32_t               grid_size
) -> void
{
    PRECOND(grid_size > 0);

    std::atomic_bool running{ true };
    MainThread       main_thread;

    std::jthread render_thread{
        [&app,
         &model_filepath,
         limit_fps,
         disable_culling,
         grid_size,
         this,
         &running,
         &main_thread] -> void
        {
            run_main_worker(
                app,
                running,
                main_thread,
                model_filepath,
                limit_fps,
                disable_culling,
                grid_size
            );
        },
    };
    run_main_thread_loop(running, main_thread);

    render_thread.join();
}

auto Context::run_main_worker(
    kiln::app::App&              app,
    std::atomic_bool&            running,
    MainThread&                  main_thread,
    const std::filesystem::path& model_filepath,
    const bool                   limit_fps,
    const bool                   disable_culling,
    const uint32_t               grid_size
) -> void
{
    const auto& config{ app.registry().at<kiln::app::Config>() };
    const auto& vulkan_instance{ app.registry().at<kiln::gfx::vulkan::Instance>() };
    const auto& render_device{ app.registry().at<kiln::gfx::renderer::Device>() };
    auto&       queue_provider{ app.registry().at<kiln::gfx::renderer::QueueProvider>() };
    auto&       render_allocator{ app.registry().at<kiln::gfx::renderer::Allocator>() };
    auto&       gltf_parser{ app.registry().at<kiln::gfx::asset::gltf::Parser>() };

    const auto  scene_load_start_time{ std::chrono::steady_clock::now() };
    const Scene scene{
        load_scene(
            render_device,
            render_allocator,
            m_staging_stream,
            gltf_parser,
            model_filepath,
            disable_culling,
            grid_size,
            *std::pmr::get_default_resource()
        ),
    };
    const auto scene_load_finish_time{ std::chrono::steady_clock::now() };
    std::println(
        "Loading the scene took {}",
        scene_load_finish_time - scene_load_start_time
    );


    kiln::wsi::WindowProxy window{ create_window(config, main_thread) };
    if (window == kiln::wsi::WindowHandle{ nullptr })
    {
        running = false;
        return;
    }
    kiln::gfx::renderer::RenderSurface render_surface{
        kiln::gfx::vulkan::check_result(
            window.create_vulkan_surface(vulkan_instance.get())
        ),
        render_device,
        std::max(number_of_frames_in_flight, 3u),
        true,
        window.framebuffer_size(),
    };
    Renderer renderer{
        render_device,
        queue_provider,
        render_allocator,
        number_of_frames_in_flight,
        render_surface.surface_format().format,
        *render_surface.extent(),
        render_surface.number_of_images(),
        disable_culling,
    };


    run_render_loop(
        config,
        render_device,
        queue_provider,
        render_allocator,
        running,
        main_thread,
        scene,
        window,
        render_surface,
        renderer,
        limit_fps,
        grid_size * 2 - 1
    );

    kiln::gfx::vulkan::check_result(render_device.logical_device().waitIdle());
}

auto Context::create_window(const kiln::app::Config& config, MainThread& main_thread)
    -> kiln::wsi::WindowProxy
{
    std::promise<kiln::wsi::WindowProxy> window_promise;

    [[maybe_unused]]
    const bool success = main_thread.work_queue.try_push(
        MainThread::Task{
            [title = config.app_name(),
             &window_promise](const MainThread& u_main_thread) -> void
            {
                const kiln::util::ScopeFail failure_guard{
                    [&window_promise, &u_main_thread] noexcept -> void
                    {
                        window_promise.set_value(
                            kiln::wsi::WindowProxy{
                                u_main_thread.wsi_engine.context(),
                                kiln::wsi::WindowHandle{ nullptr },
                            }
                        );
                    },
                };

                constexpr static kiln::wsi::WindowedWindowSettings screen_settings{
                    .content_size{ .width = 1'280, .height = 720 }
                };

                const kiln::wsi::WindowHandle window_handle{
                    u_main_thread.wsi_engine.create_window(title, screen_settings)
                };
                kiln::wsi::set_cursor_mode(
                    u_main_thread.wsi_engine.context(),
                    window_handle,
                    kiln::wsi::CursorMode::eDisabledRaw
                );

                window_promise.set_value(
                    kiln::wsi::WindowProxy{
                        u_main_thread.wsi_engine.context(),
                        window_handle,
                    }
                );
            },
        }
    );
    assert(success);

    main_thread.wsi_engine.post_empty_event();

    return window_promise.get_future().get();
}

auto Context::run_render_loop(
    const kiln::app::Config&            config,
    const kiln::gfx::renderer::Device&  render_device,
    kiln::gfx::renderer::QueueProvider& queue_provider,
    kiln::gfx::renderer::Allocator&     render_allocator,
    std::atomic_bool&                   running,
    MainThread&                         main_thread,
    const Scene&                        scene,
    kiln::wsi::WindowProxy&             window,
    kiln::gfx::renderer::RenderSurface& render_surface,
    Renderer&                           renderer,
    const bool                          limit_fps,
    const double                        movement_speed
) -> void
{
    using std::chrono_literals::operator""ms;
    constexpr static auto target_frame_duration = 1'000ms / 60;

    auto     last_fps_display_time{ std::chrono::steady_clock::now() };
    uint32_t frame_count{};

    Camera     camera;
    Controller controller{ window, movement_speed };

    while (running)
    {
        const auto frame_start_time{ std::chrono::steady_clock::now() };


        m_wsi_event_buffer.clear();
        m_wsi_event_recorder.flush(m_wsi_event_buffer);

        main_thread.event_queue.pop_all(
            [this, frame_start_time](
                kiln::wsi::Event&&           event,
                const kiln::event::Timestamp timestamp
            ) -> void
            {
                if (timestamp <= frame_start_time)
                {
                    m_wsi_event_buffer.insert(std::move(event), timestamp);
                }
                else
                {
                    m_wsi_event_recorder.record(std::move(event), timestamp);
                }
            }
        );

        for (const auto& [event, timestamp] : m_wsi_event_buffer)
        {
            window.update(event);
            controller.update(event, timestamp, window);

            if (event.type == kiln::wsi::EventType::eKeyPressedEvent
                && event.key_pressed_event.key == kiln::wsi::Key::eEscape)
            {
                running = false;
            }
        }

        ++frame_count;
        if (const auto fps_delta{ frame_start_time - last_fps_display_time };
            fps_delta > std::chrono::seconds{ 1 })
        {
            window.set_title(
                std::format(
                    "{} - {} fps",
                    config.app_name(),
                    static_cast<uint32_t>(
                        frame_count
                        / std::chrono::duration_cast<std::chrono::duration<double>>(
                              fps_delta
                        )
                              .count()
                    )

                )
            );
            last_fps_display_time = frame_start_time;
            frame_count           = 0;
        }

        window.flush_changes(
            [&](auto window_commands) -> void
            {
                [[maybe_unused]]
                const auto number_of_window_commands{ std::ranges::size(window_commands) };
                [[maybe_unused]]
                const auto success_count = main_thread.work_queue.try_append_range(
                    window_commands
                    | std::views::transform(
                        [](kiln::wsi::WindowCommand&& window_command) -> MainThread::Task
                        {
                            return MainThread::Task{
                                [x_window_command = std::move(window_command)](
                                    const MainThread& u_main_thread
                                ) mutable -> void
                                {
                                    std::move(x_window_command)(
                                        u_main_thread.wsi_engine.context()
                                    );
                                },
                            };
                        }
                    )
                );
                assert(success_count == number_of_window_commands);
                if (number_of_window_commands > 0)
                {
                    main_thread.wsi_engine.post_empty_event();
                }
            }
        );
        if (window.should_close())
        {
            running = false;
            main_thread.wsi_engine.post_empty_event();
        }

        if (window.framebuffer_size() != render_surface.extent())
        {
            kiln::gfx::vulkan::check_result(render_device.logical_device().waitIdle());
            render_surface.resize(window.framebuffer_size());
            renderer.resize(render_device, render_allocator, *render_surface.extent());
        }


        controller.update(frame_start_time);
        controller.update(camera);

        renderer.render(
            render_device,
            queue_provider,
            render_surface,
            scene,
            camera,
            *std::pmr::get_default_resource()
        );


        const auto frame_finish_time{ std::chrono::steady_clock::now() };
        if (const auto frame_duration{ frame_finish_time - frame_start_time };
            limit_fps && frame_duration < target_frame_duration)
        {
            std::this_thread::sleep_for(target_frame_duration - frame_duration);
        }
    }
}

auto Context::run_main_thread_loop(
    const std::atomic_bool& running,
    MainThread&             main_thread
) -> void
{
    while (running)
    {
        main_thread.wsi_engine.wait_events();

        for (
            std::optional<MainThread::Task> task{ main_thread.work_queue.try_pop() };
            task.has_value();
            task = main_thread.work_queue.try_pop()
        )
        {
            std::move (*task)(main_thread);
        }
    }
}

}   // namespace demo

auto kiln::reg::EntryTraits<demo::Context>::describe_build(
    BuildDirector<demo::Context>& build_director
) -> void
{
    build_director.use_builder<demo::ContextBuilder>();
}
