module;

#include <array>
#include <filesystem>
#include <ranges>
#include <source_location>
#include <span>
#include <vector>

module examples.hello_triangle;

import vulkan;

import kiln;

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
        kiln::gfx::renderer::QueueProviderBuilder& queue_provider_builder,
        const kiln::gfx::renderer::PresentationContextBuilder&,
        const kiln::gfx::renderer::PipelineContextBuilder&
    ) -> ContextBuilder
    {
        queue_provider_builder.require_queue(kiln::gfx::renderer::QueueType::eGraphics);

        return ContextBuilder{};
    }

    [[nodiscard]]
    // ReSharper disable once CppDeclaratorNeverUsed
    static auto build(
        kiln::app::MemoryArena&             memory_arena,
        const kiln::app::Config&            config,
        const kiln::gfx::vulkan::Instance&  vulkan_instance,
        const kiln::wsi::Context&           wsi_context,
        const kiln::gfx::renderer::Device&  render_device,
        kiln::gfx::renderer::QueueProvider& render_queue_provider
    ) -> Context
    {
        return Context{
            std::allocator_arg,
            memory_arena.pool_allocator(),
            config,
            vulkan_instance,
            wsi_context,
            render_device,
            render_queue_provider,
        };
    }
};

auto describe_injection(kiln::reg::BuildDirector<ContextBuilder>& build_director) -> void
{
    build_director.use_function<ContextBuilder::create>();
}

[[nodiscard]]
auto create_window(const kiln::app::Config& config, const kiln::wsi::Context& wsi_context)
    -> kiln::wsi::Window
{
    constexpr static kiln::wsi::WindowedWindowSettings screen_settings{
        .content_size{ .width = 640, .height = 480 }
    };
    const kiln::wsi::Window::CreateInfo window_info{
        .title    = config.app_name(),
        .settings = screen_settings,
    };

    return kiln::wsi::Window{ wsi_context, window_info };
}

[[nodiscard]]
auto create_graphics_command_pool(
    const kiln::gfx::renderer::Device&           render_device,
    const kiln::gfx::renderer::GraphicsQueueRef& graphics_queue
) -> kiln::gfx::renderer::GraphicsCommandPool
{
    return kiln::gfx::renderer::GraphicsCommandPool{
        render_device,
        graphics_queue.family_index(),
        kiln::gfx::renderer::CommandPoolFlags::eResettable,
    };
}

[[nodiscard]]
auto create_graphics_command_pools(
    const std::pmr::polymorphic_allocator<>&    allocator,
    const kiln::gfx::renderer::Device&          render_device,
    const kiln::gfx::renderer::GraphicsQueueRef graphics_queue,
    const uint8_t                               number_of_frames
) -> std::pmr::vector<kiln::gfx::renderer::GraphicsCommandPool>
{
    std::pmr::vector<kiln::gfx::renderer::GraphicsCommandPool> result{ allocator };
    result.reserve(number_of_frames);

    for (auto _ : std::views::repeat(std::ignore, number_of_frames))
    {
        result.push_back(create_graphics_command_pool(render_device, graphics_queue));
    }

    return result;
}

[[nodiscard]]
auto allocate_command_buffers(
    const std::pmr::polymorphic_allocator<>&                    allocator,
    std::pmr::vector<kiln::gfx::renderer::GraphicsCommandPool>& command_pools
) -> std::pmr::vector<kiln::gfx::renderer::GraphicsCommandBuffer>
{
    std::pmr::vector<kiln::gfx::renderer::GraphicsCommandBuffer> result{ allocator };
    result.reserve(command_pools.size());

    for (const std::size_t index : std::views::iota(0uz, command_pools.size()))
    {
        result.push_back(
            command_pools[index]
                .allocate_primary(kiln::gfx::renderer::CommandBufferUsageFlags::eReusable)
        );
    }

    return result;
}

[[nodiscard]]
auto create_per_frame_semaphores(
    const std::pmr::polymorphic_allocator<>& allocator,
    const kiln::gfx::renderer::Device&       device,
    const uint32_t                           number_of_frames
) -> std::pmr::vector<vk::raii::Semaphore>
{
    std::pmr::vector<vk::raii::Semaphore> result{ allocator };
    result.reserve(number_of_frames);

    for (const auto _ : std::views::repeat(std::ignore, number_of_frames))
    {
        result.push_back(
            kiln::gfx::vulkan::check_result(
                device.logical_device().createSemaphore(vk::SemaphoreCreateInfo{})
            )
        );
    }

    return result;
}

[[nodiscard]]
auto create_per_frame_fences(
    const std::pmr::polymorphic_allocator<>& allocator,
    const kiln::gfx::renderer::Device&       device,
    const uint8_t                            number_of_frames
) -> std::pmr::vector<vk::raii::Fence>
{
    std::pmr::vector<vk::raii::Fence> result{ allocator };
    result.reserve(number_of_frames);

    for (const auto _ : std::views::repeat(std::ignore, number_of_frames))
    {
        result.push_back(
            kiln::gfx::vulkan::check_result(device.logical_device().createFence(
                vk::FenceCreateInfo{ .flags = vk::FenceCreateFlagBits::eSignaled }
            ))
        );
    }

    return result;
}

Context::Context(Context&& other, const allocator_type& allocator)
    : m_render_device_ref{ std::move(other.m_render_device_ref) },
      m_render_queue_provider_ref{ std::move(other.m_render_queue_provider_ref) },
      m_number_of_frames{ other.m_number_of_frames },
      m_current_frame_index{ other.m_current_frame_index },
      m_window{ std::move(other.m_window) },
      m_surface{ std::move(other.m_surface) },
      m_pipeline_layout{ std::move(other.m_pipeline_layout) },
      m_shader_module{ std::move(other.m_shader_module) },
      m_pipeline{ std::move(other.m_pipeline) },
      m_graphics_command_pools{ std::move(other.m_graphics_command_pools), allocator },
      m_graphics_command_buffers{ std::move(other.m_graphics_command_buffers),
                                  allocator },
      m_image_acquired_semaphores{ std::move(other.m_image_acquired_semaphores),
                                   allocator },
      m_render_finished_semaphores{ std::move(other.m_render_finished_semaphores),
                                    allocator },
      m_render_finished_fences{ std::move(other.m_render_finished_fences), allocator }
{
}

Context::Context(
    const kiln::app::Config&            config,
    const kiln::gfx::vulkan::Instance&  vulkan_instance,
    const kiln::wsi::Context&           wsi_context,
    const kiln::gfx::renderer::Device&  render_device,
    kiln::gfx::renderer::QueueProvider& render_queue_provider
)
    : Context{
          std::allocator_arg,
          std::pmr::get_default_resource(),
          config,
          vulkan_instance,
          wsi_context,
          render_device,
          render_queue_provider,
      }
{
}

Context::Context(
    const std::allocator_arg_t,
    const allocator_type&               allocator,
    const kiln::app::Config&            config,
    const kiln::gfx::vulkan::Instance&  vulkan_instance,
    const kiln::wsi::Context&           wsi_context,
    const kiln::gfx::renderer::Device&  render_device,
    kiln::gfx::renderer::QueueProvider& render_queue_provider
)
    : m_render_device_ref{
          render_device
},
      m_render_queue_provider_ref{ render_queue_provider },
      m_window{ create_window(config, wsi_context) },
      m_surface{
          kiln::gfx::vulkan::check_result(
              m_window.create_vulkan_surface(vulkan_instance.get())
          ),
          render_device,
          m_number_of_frames,
          true,
          m_window.framebuffer_size(),
      },
      m_pipeline_layout{
          kiln::gfx::vulkan::check_result(
              render_device.logical_device().createPipelineLayout({})
          ),
      },
      m_shader_module{
          *kiln::gfx::renderer::ShaderModule::load_from_file(
              std::filesystem::path{ std::source_location::current().file_name() }
                  .parent_path()
              / "shaders"
              / "triangle.spv"
          ),
      },
      m_pipeline{
          kiln::gfx::renderer::GraphicsPipelineBuilder{
              m_pipeline_layout,
              m_shader_module,
              m_shader_module,
          }
              .set_color_formats(std::span{ &m_surface.surface_format().format, 1 })
              .build(render_device)
      },
      m_graphics_command_pools{
          create_graphics_command_pools(
              allocator,
              render_device,
              *m_render_queue_provider_ref.get().graphics_queue(),
              m_number_of_frames
          ),
      },
      m_graphics_command_buffers{
          allocate_command_buffers(allocator, m_graphics_command_pools)
      },
      m_image_acquired_semaphores{
          create_per_frame_semaphores(allocator, render_device, m_number_of_frames)
      },
      m_render_finished_semaphores{
          create_per_frame_semaphores(
              allocator,
              render_device,
              m_surface.number_of_images()
          ),
      },
      m_render_finished_fences{
          create_per_frame_fences(allocator, render_device, m_number_of_frames)
      }
{
}

auto Context::get_allocator() const -> allocator_type
{
    return m_graphics_command_pools.get_allocator();
}

auto Context::window() noexcept -> kiln::wsi::Window&
{
    return m_window;
}

auto Context::on_window_resize(const kiln::wsi::Size2u new_resolution) -> void
{
    if (m_surface.extent() == new_resolution)
    {
        return;
    }

    kiln::gfx::vulkan::check_result(m_render_device_ref.get().logical_device().waitIdle());
    m_surface.resize(new_resolution);
}

auto Context::render(std::pmr::memory_resource& transient_memory_resource) -> void
{
    kiln::gfx::vulkan::check_result(
        m_render_device_ref.get().logical_device().waitForFences(
            *m_render_finished_fences[m_current_frame_index],
            vk::True,
            std::numeric_limits<uint64_t>::max()
        )
    );

    const std::optional<uint32_t> swapchain_image_index
        = m_surface.acquire_image(m_image_acquired_semaphores[m_current_frame_index]);
    if (!swapchain_image_index.has_value())
    {
        return;
    }

    m_render_device_ref.get().logical_device().resetFences(
        *m_render_finished_fences[m_current_frame_index]
    );

    m_graphics_command_pools[m_current_frame_index].reset();
    kiln::gfx::renderer::GraphicsCommandBuffer& graphics_command_buffer{
        m_graphics_command_buffers[m_current_frame_index]
    };

    graphics_command_buffer.begin_recording();

    const vk::ImageMemoryBarrier2 render_image_memory_barrier {
            .srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
            .oldLayout = vk::ImageLayout::eUndefined,
            .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .image = m_surface.image_at(*swapchain_image_index),
            .subresourceRange = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .levelCount = 1,
                .layerCount = 1,
              },
        };
    const kiln::gfx::renderer::DependencyInfo render_dependency_info{
        .image_memory_barriers = std::span{ &render_image_memory_barrier, 1 },
    };
    graphics_command_buffer.record_barrier(render_dependency_info);

    const vk::Rect2D render_area{
        .extent = *m_surface.extent(),
    };
    const kiln::gfx::renderer::RenderPass render_pass{
        render_area,
        std::array{
            kiln::gfx::renderer::ColorAttachment{
                m_surface.image_view_at(*swapchain_image_index),
            }
                .set_clear_value(std::array{ 0.01f, 0.01f, 0.01f, 1.f })   //
        }
    };
    graphics_command_buffer.record_render_pass_start(render_pass);

    graphics_command_buffer.record_pipeline_bind(m_pipeline);
    graphics_command_buffer.record_draw(3);
    graphics_command_buffer.record_render_pass_finish();

    const vk::ImageMemoryBarrier2 present_image_memory_barrier {
            .srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
            .dstStageMask = vk::PipelineStageFlagBits2::eAllCommands,
            .oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .newLayout = vk::ImageLayout::ePresentSrcKHR,
            .image = m_surface.image_at(*swapchain_image_index),
            .subresourceRange = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
              },
        };
    const kiln::gfx::renderer::DependencyInfo present_dependency_info{
        .image_memory_barriers = std::span{ &present_image_memory_barrier, 1 },
    };
    graphics_command_buffer.record_barrier(present_dependency_info);

    graphics_command_buffer.end_recording();

    const vk::SemaphoreSubmitInfo render_wait_semaphore_info{
        .semaphore = m_image_acquired_semaphores[m_current_frame_index],
        .stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
    };
    const vk::SemaphoreSubmitInfo render_finished_semaphore_info{
        .semaphore = m_render_finished_semaphores[*swapchain_image_index],
        .stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
    };
    kiln::gfx::renderer::SubmitInfo submit_info{ &transient_memory_resource };
    submit_info.wait_semaphores().push_back(render_wait_semaphore_info);
    submit_info.signal_semaphores().push_back(render_finished_semaphore_info);
    submit_info.fence() = *m_render_finished_fences[m_current_frame_index];
    m_render_queue_provider_ref.get().graphics_queue()->submit(
        graphics_command_buffer,
        submit_info
    );

    m_surface.present(
        *m_render_queue_provider_ref.get()
             .graphics_queue_as<kiln::gfx::renderer::PresentQueueRef>(),
        *swapchain_image_index,
        std::span{ &*m_render_finished_semaphores[*swapchain_image_index], 1 }
    );

    m_current_frame_index = (m_current_frame_index + 1) % m_number_of_frames;
}

// ReSharper disable once CppMemberFunctionMayBeConst
auto Context::shut_down() -> void
{
    kiln::gfx::vulkan::check_result(m_render_device_ref.get().logical_device().waitIdle());
}

}   // namespace demo

auto kiln::reg::EntryTraits<demo::Context>::describe_build(
    BuildDirector<demo::Context>& build_director
) -> void
{
    build_director.use_builder<demo::ContextBuilder>();
}
