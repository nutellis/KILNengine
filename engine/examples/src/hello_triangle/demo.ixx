module;

#include <functional>
#include <memory_resource>
#include <vector>

export module examples.hello_triangle;

import vulkan;

import kiln.app.config.Config;
import kiln.gfx.renderer.command.GraphicsCommandBuffer;
import kiln.gfx.renderer.command.GraphicsCommandPool;
import kiln.gfx.renderer.command.QueueProvider;
import kiln.gfx.renderer.device.Device;
import kiln.gfx.renderer.pipeline.GraphicsPipeline;
import kiln.gfx.renderer.pipeline.ShaderModule;
import kiln.gfx.renderer.presentation.RenderSurface;
import kiln.gfx.vulkan.Instance;
import kiln.reg.BuildDirector;
import kiln.reg.EntryTraits;
import kiln.wsi.Context;
import kiln.wsi.Size;
import kiln.wsi.Window;

namespace demo {

export class Context {
public:
    // required for interfacing with the standard
    using allocator_type = std::pmr::polymorphic_allocator<>;


    Context(Context&&, const allocator_type& allocator);

    explicit Context(
        const kiln::app::Config&            config,
        const kiln::gfx::vulkan::Instance&  vulkan_instance,
        const kiln::wsi::Context&           wsi_context,
        const kiln::gfx::renderer::Device&  render_device,
        kiln::gfx::renderer::QueueProvider& render_queue_provider
    );
    explicit Context(
        std::allocator_arg_t,
        const allocator_type&               allocator,
        const kiln::app::Config&            config,
        const kiln::gfx::vulkan::Instance&  vulkan_instance,
        const kiln::wsi::Context&           wsi_context,
        const kiln::gfx::renderer::Device&  render_device,
        kiln::gfx::renderer::QueueProvider& render_queue_provider
    );


    // required for interfacing with the standard
    [[nodiscard]]
    auto get_allocator() const -> allocator_type;

    [[nodiscard]]
    auto window() noexcept -> kiln::wsi::Window&;

    auto on_window_resize(kiln::wsi::Size2u new_resolution) -> void;

    auto render(
        std::pmr::memory_resource& transient_memory_resource
        = *std::pmr::get_default_resource()
    ) -> void;

    auto shut_down() -> void;

private:
    std::reference_wrapper<const kiln::gfx::renderer::Device> m_render_device_ref;
    std::reference_wrapper<kiln::gfx::renderer::QueueProvider> m_render_queue_provider_ref;
    uint8_t                                                    m_number_of_frames{ 2 };
    uint8_t                                                    m_current_frame_index{};
    kiln::wsi::Window                                          m_window;
    kiln::gfx::renderer::RenderSurface                         m_surface;
    vk::raii::PipelineLayout                                   m_pipeline_layout;
    kiln::gfx::renderer::ShaderModule                          m_shader_module;
    kiln::gfx::renderer::GraphicsPipeline                      m_pipeline;
    std::pmr::vector<kiln::gfx::renderer::GraphicsCommandPool> m_graphics_command_pools;
    std::pmr::vector<kiln::gfx::renderer::GraphicsCommandBuffer>
                                          m_graphics_command_buffers;
    std::pmr::vector<vk::raii::Semaphore> m_image_acquired_semaphores;
    std::pmr::vector<vk::raii::Semaphore> m_render_finished_semaphores;
    std::pmr::vector<vk::raii::Fence>     m_render_finished_fences;
};

}   // namespace demo

template <>
struct kiln::reg::EntryTraits<demo::Context> {
    static auto describe_build(BuildDirector<demo::Context>& build_director) -> void;
};
