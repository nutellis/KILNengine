module;

#include <array>
#include <filesystem>
#include <memory_resource>
#include <optional>
#include <ranges>
#include <source_location>
#include <utility>

#include <vk_mem_alloc.h>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/quaternion_common.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/gtc/quaternion.hpp>

#include "kiln/util/contract_macros.hpp"

module examples.frustum_culling.workflow.Renderer;

import vulkan;

import kiln.gfx.renderer.command.CommandBufferBase;
import kiln.gfx.renderer.command.CommandPoolFlags;
import kiln.gfx.renderer.command.DependencyInfo;
import kiln.gfx.renderer.command.PresentQueueRef;
import kiln.gfx.renderer.command.SubmitInfo;
import kiln.gfx.renderer.memory.Allocator;
import kiln.gfx.renderer.memory.Image;
import kiln.gfx.renderer.pipeline.ColorAttachment;
import kiln.gfx.renderer.pipeline.ComputePipelineBuilder;
import kiln.gfx.renderer.pipeline.DepthAttachment;
import kiln.gfx.renderer.pipeline.GraphicsPipelineBuilder;
import kiln.gfx.renderer.pipeline.RenderPass;
import kiln.gfx.vulkan.result.check_result;
import kiln.util.contracts;

import examples.frustum_culling.shaders;

namespace demo {

Renderer::Renderer(Renderer&& other, [[maybe_unused]] const allocator_type& allocator)
    : Renderer{ std::move(other) }
{
    PRECOND(get_allocator() == allocator);
}

Renderer::Renderer(
    const kiln::gfx::renderer::Device&  device,
    kiln::gfx::renderer::QueueProvider& queue_provider,
    kiln::gfx::renderer::Allocator&     render_allocator,
    const uint32_t                      number_of_frames_in_flight,
    const vk::Format                    swapchain_surface_format,
    const vk::Extent2D&                 swapchain_surface_extent,
    const uint32_t                      number_of_swapchain_images,
    const bool                          disable_culling
)
    : Renderer{
          std::allocator_arg,
          std::pmr::get_default_resource(),
          device,
          queue_provider,
          render_allocator,
          number_of_frames_in_flight,
          swapchain_surface_format,
          swapchain_surface_extent,
          number_of_swapchain_images,
          disable_culling,
      }
{
}

[[nodiscard]]
auto create_compute_command_pools(
    const std::pmr::polymorphic_allocator<>& allocator,
    const kiln::gfx::renderer::Device&       render_device,
    kiln::gfx::renderer::QueueProvider&      queue_provider,
    const uint32_t                           count
) -> std::pmr::vector<kiln::gfx::renderer::ComputeCommandPool>
{
    std::pmr::vector<kiln::gfx::renderer::ComputeCommandPool> result{ allocator };
    result.reserve(count);

    // ReSharper disable once CppDFAUnreadVariable
    for (auto _ : std::views::repeat(std::ignore, count))
    {
        result.emplace_back(
            render_device,
            queue_provider.graphics_queue()->family_index(),
            kiln::gfx::renderer::CommandPoolFlags::eResettable
        );
    }

    return result;
}

[[nodiscard]]
auto allocate_command_buffers(
    const std::pmr::polymorphic_allocator<>&                   allocator,
    std::pmr::vector<kiln::gfx::renderer::ComputeCommandPool>& command_pools
) -> std::pmr::vector<kiln::gfx::renderer::ComputeCommandBuffer>
{
    std::pmr::vector<kiln::gfx::renderer::ComputeCommandBuffer> result{ allocator };
    result.reserve(command_pools.size());

    for (const std::size_t index : std::views::iota(0uz, command_pools.size()))
    {
        result.push_back(command_pools[index].allocate_primary());
    }

    return result;
}

[[nodiscard]]
auto create_graphics_command_pools(
    const std::pmr::polymorphic_allocator<>& allocator,
    const kiln::gfx::renderer::Device&       render_device,
    kiln::gfx::renderer::QueueProvider&      queue_provider,
    const uint32_t                           count
) -> std::pmr::vector<kiln::gfx::renderer::GraphicsCommandPool>
{
    std::pmr::vector<kiln::gfx::renderer::GraphicsCommandPool> result{ allocator };
    result.reserve(count);

    // ReSharper disable once CppDFAUnreadVariable
    for (auto _ : std::views::repeat(std::ignore, count))
    {
        result.emplace_back(
            render_device,
            queue_provider.graphics_queue()->family_index(),
            kiln::gfx::renderer::CommandPoolFlags::eResettable
        );
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
        result.push_back(command_pools[index].allocate_primary());
    }

    return result;
}

[[nodiscard]]
auto create_per_frame_semaphores(
    const std::pmr::polymorphic_allocator<>& allocator,
    const kiln::gfx::renderer::Device&       device,
    const uint32_t                           count
) -> std::pmr::vector<vk::raii::Semaphore>
{
    std::pmr::vector<vk::raii::Semaphore> result{ allocator };
    result.reserve(count);

    for (const auto _ : std::views::repeat(std::ignore, count))
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
    const uint32_t                           count
) -> std::pmr::vector<vk::raii::Fence>
{
    std::pmr::vector<vk::raii::Fence> result{ allocator };
    result.reserve(count);

    for (const auto _ : std::views::repeat(std::ignore, count))
    {
        result.push_back(
            kiln::gfx::vulkan::check_result(device.logical_device().createFence(
                vk::FenceCreateInfo{ .flags = vk::FenceCreateFlagBits::eSignaled }
            ))
        );
    }

    return result;
}

[[nodiscard]]
auto create_frustum_culling_pipeline_layout(const vk::raii::Device& device)
    -> vk::raii::PipelineLayout
{
    constexpr static vk::PushConstantRange push_constant_range{
        .stageFlags = vk::ShaderStageFlagBits::eCompute,
        .size       = sizeof(shaders::FrustumCullingPushConstants),
    };

    constexpr static vk::PipelineLayoutCreateInfo create_info{
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &push_constant_range,
    };

    return kiln::gfx::vulkan::check_result(device.createPipelineLayout(create_info));
}

[[nodiscard]]
auto create_draw_command_generation_pipeline_layout(const vk::raii::Device& device)
    -> vk::raii::PipelineLayout
{
    constexpr static vk::PushConstantRange push_constant_range{
        .stageFlags = vk::ShaderStageFlagBits::eCompute,
        .size       = sizeof(shaders::DrawCommandGenerationPushConstants),
    };

    constexpr static vk::PipelineLayoutCreateInfo create_info{
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &push_constant_range,
    };

    return kiln::gfx::vulkan::check_result(device.createPipelineLayout(create_info));
}

[[nodiscard]]
auto create_instance_index_generation_pipeline_layout(const vk::raii::Device& device)
    -> vk::raii::PipelineLayout
{
    constexpr static vk::PushConstantRange push_constant_range{
        .stageFlags = vk::ShaderStageFlagBits::eCompute,
        .size       = sizeof(shaders::InstanceIndexGenerationPushConstants),
    };

    constexpr static vk::PipelineLayoutCreateInfo create_info{
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &push_constant_range,
    };

    return kiln::gfx::vulkan::check_result(device.createPipelineLayout(create_info));
}

[[nodiscard]]
auto create_graphics_pipeline_layout(const vk::raii::Device& device)
    -> vk::raii::PipelineLayout
{
    constexpr static std::array push_constant_ranges{
        vk::PushConstantRange{ .stageFlags = vk::ShaderStageFlagBits::eVertex,
                              .size = sizeof(shaders::Scene) + sizeof(shaders::Camera) },
    };

    constexpr static vk::PipelineLayoutCreateInfo create_info{
        .pushConstantRangeCount = push_constant_ranges.size(),
        .pPushConstantRanges    = push_constant_ranges.data(),
    };

    return kiln::gfx::vulkan::check_result(device.createPipelineLayout(create_info));
}

const auto shader_kernel_directory{
    std::filesystem::path{ std::source_location::current().file_name() }
            .parent_path()
            .parent_path()
        / "shaders"
        / "kernels",
};

[[nodiscard]]
auto pick_depth_format(const vk::raii::PhysicalDevice& physical_device) -> vk::Format
{
    for (
        const vk::Format format :
        { vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint }
    )
    {
        if (physical_device.getFormatProperties2(format)
                .formatProperties.optimalTilingFeatures
            & vk::FormatFeatureFlagBits::eDepthStencilAttachment)
        {
            return format;
        }
    }
    std::unreachable();
}

[[nodiscard]]
auto create_depth_image(
    const kiln::gfx::renderer::Device& device,
    kiln::gfx::renderer::Allocator&    allocator,
    const vk::Extent2D&                extent
) -> kiln::gfx::renderer::Image
{
    const vk::ImageCreateInfo create_info{
        .imageType = vk::ImageType::e2D,
        .format    = pick_depth_format(device.physical_device()),
        .extent{ .width = extent.width, .height = extent.height, .depth = 1 },
        .mipLevels   = 1,
        .arrayLayers = 1,
        .samples     = vk::SampleCountFlagBits::e1,
        .tiling      = vk::ImageTiling::eOptimal,
        .usage       = vk::ImageUsageFlagBits::eDepthStencilAttachment,
    };
    constexpr static VmaAllocationCreateInfo allocation_create_info{
        .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO
    };
    return allocator.create_image(create_info, allocation_create_info);
}

[[nodiscard]]
auto create_depth_image_view(
    const kiln::gfx::renderer::Device& device,
    kiln::gfx::renderer::Image&        depth_image
) -> vk::raii::ImageView
{
    const vk::ImageViewCreateInfo create_info{
        .image    = depth_image.get(),
        .viewType = vk::ImageViewType::e2D,
        .format   = depth_image.format(),
        .subresourceRange{ .aspectMask = vk::ImageAspectFlagBits::eDepth,
                          .levelCount = 1,
                          .layerCount = 1 },
    };
    return kiln::gfx::vulkan::check_result(
        device.logical_device().createImageView(create_info)
    );
}

Renderer::Renderer(
    std::allocator_arg_t,
    const allocator_type&               allocator,
    const kiln::gfx::renderer::Device&  device,
    kiln::gfx::renderer::QueueProvider& queue_provider,
    kiln::gfx::renderer::Allocator&     render_allocator,
    const uint32_t                      number_of_frames_in_flight,
    const vk::Format                    swapchain_surface_format,
    const vk::Extent2D&                 swapchain_surface_extent,
    const uint32_t                      number_of_swapchain_images,
    const bool                          disable_culling
)
    : m_disable_culling{
          disable_culling
},
      m_compute_command_pools{
          create_compute_command_pools(
              allocator,
              device,
              queue_provider,
              number_of_frames_in_flight
          ),
      },
      m_frustum_culling_command_buffers{
          allocate_command_buffers(allocator, m_compute_command_pools)
      },
      m_draw_command_generation_command_buffers{
          allocate_command_buffers(allocator, m_compute_command_pools)
      },
      m_instance_index_generation_command_buffers{
          allocate_command_buffers(allocator, m_compute_command_pools)
      },
      m_graphics_command_pools{
          create_graphics_command_pools(
              allocator,
              device,
              queue_provider,
              number_of_frames_in_flight
          ),
      },
      m_graphics_command_buffers{
          allocate_command_buffers(allocator, m_graphics_command_pools)
      },
      m_image_acquired_semaphores{
          create_per_frame_semaphores(allocator, device, number_of_frames_in_flight)
      },
      m_render_finished_semaphores{
          create_per_frame_semaphores(allocator, device, number_of_swapchain_images)
      },
      m_render_finished_fences{
          create_per_frame_fences(allocator, device, number_of_frames_in_flight)
      },
      m_depth_image{
          create_depth_image(device, render_allocator, swapchain_surface_extent),
      },
      m_depth_image_view{ create_depth_image_view(device, m_depth_image) },
      m_frustum_culling_pipeline_layout{
          create_frustum_culling_pipeline_layout(device.logical_device()),
      },
      m_draw_command_generation_pipeline_layout{
          create_draw_command_generation_pipeline_layout(device.logical_device()),
      },
      m_instance_index_generation_pipeline_layout{
          create_instance_index_generation_pipeline_layout(device.logical_device()),
      },
      m_graphics_pipeline_layout{
          create_graphics_pipeline_layout(device.logical_device())
      },
      m_frustum_culling_shader_module{
          *kiln::gfx::renderer::ShaderModule::load_from_file(
              shader_kernel_directory / "frustum_cull.spv"
          ),
      },
      m_draw_command_generation_shader_module{
          *kiln::gfx::renderer::ShaderModule::load_from_file(
              shader_kernel_directory / "generate_draw_commands.spv"
          ),
      },
      m_instance_index_generation_shader_module{
          *kiln::gfx::renderer::ShaderModule::load_from_file(
              shader_kernel_directory / "generate_instance_indices.spv"
          ),
      },
      m_graphics_shader_module{
          *kiln::gfx::renderer::ShaderModule::load_from_file(
              shader_kernel_directory / "shade.spv"
          ),
      },
      m_frustum_culling_pipeline{
          kiln::gfx::renderer::ComputePipelineBuilder{
              m_frustum_culling_pipeline_layout,
              m_frustum_culling_shader_module,
          }
              .build(device),
      },
      m_draw_command_generation_pipeline{
          kiln::gfx::renderer::ComputePipelineBuilder{
              m_draw_command_generation_pipeline_layout,
              m_draw_command_generation_shader_module,
          }
              .build(device),
      },
      m_instance_index_generation_pipeline{
          kiln::gfx::renderer::ComputePipelineBuilder{
              m_instance_index_generation_pipeline_layout,
              m_instance_index_generation_shader_module,
          }
              .build(device),
      },
      m_graphics_pipeline{
          kiln::gfx::renderer::GraphicsPipelineBuilder{
              m_graphics_pipeline_layout,
              m_graphics_shader_module,
              m_graphics_shader_module,
          }
              .set_cull_mode(vk::CullModeFlagBits::eBack)
              .set_color_formats(std::span{ &swapchain_surface_format, 1 })
              .set_depth_format(pick_depth_format(device.physical_device()))
              .build(device)
      }
{
}

auto Renderer::get_allocator() const noexcept -> allocator_type
{
    return m_graphics_command_pools.get_allocator();
}

auto Renderer::resize(
    const kiln::gfx::renderer::Device& device,
    kiln::gfx::renderer::Allocator&    allocator,
    const vk::Extent2D&                swapchain_surface_extent
) -> void
{
    m_depth_image      = create_depth_image(device, allocator, swapchain_surface_extent);
    m_depth_image_view = create_depth_image_view(device, m_depth_image);
}

[[nodiscard]]
auto aspect_ratio_from(const vk::Extent2D frame_buffer_size) -> float
{
    return static_cast<float>(frame_buffer_size.width)
         / static_cast<float>(frame_buffer_size.height);
}

auto Renderer::render(
    const kiln::gfx::renderer::Device&  device,
    kiln::gfx::renderer::QueueProvider& queue_provider,
    kiln::gfx::renderer::RenderSurface& surface,
    const Scene&                        scene,
    const Camera&                       camera,
    std::pmr::memory_resource&          transient_memory_resource
) -> void
{
    kiln::gfx::vulkan::check_result(device.logical_device().waitForFences(
        *m_render_finished_fences[m_current_frame_index],
        vk::True,
        std::numeric_limits<uint64_t>::max()
    ));

    const std::optional<uint32_t> swapchain_image_index
        = surface.acquire_image(m_image_acquired_semaphores[m_current_frame_index]);
    if (!swapchain_image_index.has_value())
    {
        return;
    }

    device.logical_device().resetFences(*m_render_finished_fences[m_current_frame_index]);


    if (!m_disable_culling)
    {
        m_compute_command_pools[m_current_frame_index].reset();
        frustum_cull(
            queue_provider,
            scene,
            camera,
            aspect_ratio_from(*surface.extent()),
            transient_memory_resource
        );
        generate_draw_commands(queue_provider, scene, transient_memory_resource);
        generate_instance_indices(queue_provider, scene, transient_memory_resource);
    }

    m_graphics_command_pools[m_current_frame_index].reset();
    draw(
        queue_provider,
        surface,
        scene,
        camera,
        *swapchain_image_index,
        transient_memory_resource
    );


    surface.present(
        *queue_provider.graphics_queue_as<kiln::gfx::renderer::PresentQueueRef>(),
        *swapchain_image_index,
        std::span{ &*m_render_finished_semaphores[*swapchain_image_index], 1 }
    );


    m_current_frame_index = (m_current_frame_index + 1) % number_of_frames_in_flight();
}

auto Renderer::number_of_frames_in_flight() const noexcept -> uint32_t
{
    return static_cast<uint32_t>(m_graphics_command_pools.size());
}

[[nodiscard]]
auto frustum_from(const Camera& camera, const double aspect_ratio) -> shaders::Frustum
{
    const glm::dmat4 view_matrix{
        glm::translate(
            glm::mat4_cast(glm::conjugate(camera.orientation())),
            -camera.position()
        ),
    };
    const glm::dmat4 projection_matrix{
        glm::perspectiveRH_ZO(
            camera.fov(),
            aspect_ratio,
            camera.near_plane(),
            camera.far_plane()
        ),
    };
    const glm::mat4x4 view_projection_matrix{ projection_matrix * view_matrix };
    const auto        extract_plane
        = [&view_projection_matrix](const int row, const float sign) -> shaders::Plane
    {
        const glm::vec4 p{
            view_projection_matrix[0][3] + sign * view_projection_matrix[0][row],
            view_projection_matrix[1][3] + sign * view_projection_matrix[1][row],
            view_projection_matrix[2][3] + sign * view_projection_matrix[2][row],
            view_projection_matrix[3][3] + sign * view_projection_matrix[3][row],
        };

        const float length{ glm::length(glm::vec3(p)) };
        return shaders::Plane{ .normal = glm::vec3(p) / length, .offset = p.w / length };
    };
    const auto extract_near_plane = [&view_projection_matrix]() -> shaders::Plane
    {
        const glm::vec4 p{
            view_projection_matrix[0][2],
            view_projection_matrix[1][2],
            view_projection_matrix[2][2],
            view_projection_matrix[3][2],
        };

        const float length{ glm::length(glm::vec3(p)) };
        return shaders::Plane{ .normal = glm::vec3(p) / length, .offset = p.w / length };
    };

    return shaders::Frustum{
        .planes{
                extract_plane(0, 1),    // Left
            extract_plane(0, -1),   // Right
            extract_plane(1, 1),    // Bottom
            extract_plane(1, -1),   // Top
            extract_near_plane(),   // Near
            extract_plane(2, -1),   // Far
        },
    };
}

auto Renderer::frustum_cull(
    kiln::gfx::renderer::QueueProvider& queue_provider,
    const Scene&                        scene,
    const Camera&                       camera,
    const double                        aspect_ratio,
    std::pmr::memory_resource&          transient_memory_resource
) -> void
{
    if (scene.max_instance_count() == 0)
    {
        return;
    }

    kiln::gfx::renderer::ComputeCommandBuffer& command_buffer{
        m_frustum_culling_command_buffers[m_current_frame_index]
    };


    command_buffer.begin_recording();


    const vk::BufferMemoryBarrier2
        draw_command_instance_counts_clear_buffer_memory_barrier{
            .srcStageMask  = vk::PipelineStageFlagBits2::eComputeShader,
            .srcAccessMask = vk::AccessFlagBits2::eShaderRead,
            .dstStageMask  = vk::PipelineStageFlagBits2::eTransfer,
            .dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
            .buffer = scene.draw_command_instance_counts_buffer_region().buffer().get(),
            .offset = scene.draw_command_instance_counts_buffer_region().offset(),
            .size   = scene.draw_command_instance_counts_buffer_region().size(),
        };
    const kiln::gfx::renderer::DependencyInfo clear_dependency_info{
        .buffer_memory_barriers
        = std::span{ &draw_command_instance_counts_clear_buffer_memory_barrier, 1 },
    };
    command_buffer.record_barrier(clear_dependency_info);

    command_buffer
        .record_buffer_fill(scene.draw_command_instance_counts_buffer_region(), 0);

    command_buffer.record_pipeline_bind(m_frustum_culling_pipeline);

    const shaders::FrustumCullingPushConstants push_constants{
        .global_instance_count = scene.max_instance_count(),
        .instance_draw_command_indices
        = scene.instance_draw_command_indices_buffer_address(),
        .bounding_spheres = scene.instance_sphere_bounding_volumes_buffer_address(),
        .frustum          = frustum_from(camera, aspect_ratio),
        .draw_command_instance_counts
        = scene.draw_command_instance_counts_buffer_address(),
        .instance_draw_command_offsets
        = scene.instance_draw_command_offsets_buffer_address(),
    };
    const vk::PushConstantsInfo push_constants_info{
        .layout     = m_frustum_culling_pipeline_layout,
        .stageFlags = vk::ShaderStageFlagBits::eCompute,
        .size       = sizeof(decltype(push_constants)),
        .pValues    = &push_constants,
    };
    command_buffer.record_push_constants(push_constants_info);

    const std::array dispatch_buffer_memory_barriers{
        vk::BufferMemoryBarrier2{
                                 .srcStageMask  = vk::PipelineStageFlagBits2::eTransfer,
                                 .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
                                 .dstStageMask  = vk::PipelineStageFlagBits2::eComputeShader,
                                 .dstAccessMask = vk::AccessFlagBits2::eShaderWrite,
                                 .buffer = scene.draw_command_instance_counts_buffer_region().buffer().get(),
                                 .offset = scene.draw_command_instance_counts_buffer_region().offset(),
                                 .size   = scene.draw_command_instance_counts_buffer_region().size(),
                                 },
        vk::BufferMemoryBarrier2{
                                 .srcStageMask  = vk::PipelineStageFlagBits2::eComputeShader,
                                 .srcAccessMask = vk::AccessFlagBits2::eShaderRead,
                                 .dstStageMask  = vk::PipelineStageFlagBits2::eComputeShader,
                                 .dstAccessMask = vk::AccessFlagBits2::eShaderWrite,
                                 .buffer = scene.instance_draw_command_offsets_buffer_region().buffer().get(),
                                 .offset = scene.instance_draw_command_offsets_buffer_region().offset(),
                                 .size   = scene.instance_draw_command_offsets_buffer_region().size(),
                                 },
    };
    const kiln::gfx::renderer::DependencyInfo dispatch_dependency_info{
        .buffer_memory_barriers = dispatch_buffer_memory_barriers,
    };
    command_buffer.record_barrier(dispatch_dependency_info);

    command_buffer.record_dispatch((scene.max_instance_count() + 31) / 32, 1, 1);


    command_buffer.end_recording();


    const kiln::gfx::renderer::SubmitInfo submit_info{ &transient_memory_resource };
    queue_provider.graphics_queue_as<kiln::gfx::renderer::ComputeQueueRef>()
        ->submit(command_buffer, submit_info);
}

auto Renderer::generate_draw_commands(
    kiln::gfx::renderer::QueueProvider& queue_provider,
    const Scene&                        scene,
    std::pmr::memory_resource&          transient_memory_resource
) -> void
{
    if (scene.max_instance_count() == 0)
    {
        return;
    }

    kiln::gfx::renderer::ComputeCommandBuffer& command_buffer{
        m_draw_command_generation_command_buffers[m_current_frame_index]
    };


    command_buffer.begin_recording();


    const vk::BufferMemoryBarrier2 instance_counter_clear_buffer_memory_barrier{
        .srcStageMask  = vk::PipelineStageFlagBits2::eComputeShader,
        .srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
        .dstStageMask  = vk::PipelineStageFlagBits2::eTransfer,
        .dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
        .buffer        = scene.instance_counter_buffer_region().buffer().get(),
        .offset        = scene.instance_counter_buffer_region().offset(),
        .size          = scene.instance_counter_buffer_region().size(),
    };
    const kiln::gfx::renderer::DependencyInfo instance_counter_clear_dependency_info{
        .buffer_memory_barriers
        = std::span{ &instance_counter_clear_buffer_memory_barrier, 1 },
    };
    command_buffer.record_barrier(instance_counter_clear_dependency_info);

    command_buffer.record_buffer_fill(scene.instance_counter_buffer_region(), 0);


    const vk::BufferMemoryBarrier2 draw_command_count_clear_buffer_memory_barrier{
        .srcStageMask  = vk::PipelineStageFlagBits2::eDrawIndirect,
        .srcAccessMask = vk::AccessFlagBits2::eIndirectCommandRead,
        .dstStageMask  = vk::PipelineStageFlagBits2::eTransfer,
        .dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
        .buffer        = scene.draw_command_count_buffer_region().buffer().get(),
        .offset        = scene.draw_command_count_buffer_region().offset(),
        .size          = scene.draw_command_count_buffer_region().size(),
    };
    const kiln::gfx::renderer::DependencyInfo draw_command_count_clear_dependency_info{
        .buffer_memory_barriers
        = std::span{ &draw_command_count_clear_buffer_memory_barrier, 1 },
    };
    command_buffer.record_barrier(draw_command_count_clear_dependency_info);

    command_buffer.record_buffer_fill(scene.draw_command_count_buffer_region(), 0);


    command_buffer.record_pipeline_bind(m_draw_command_generation_pipeline);

    const shaders::DrawCommandGenerationPushConstants push_constants{
        .draw_command_count = scene.max_draw_command_count(),
        .original_draw_commands_buffer_address
        = scene.original_draw_commands_buffer_address(),
        .draw_command_instance_counts
        = scene.draw_command_instance_counts_buffer_address(),
        .instance_counter     = scene.instance_counter_buffer_address(),
        .draw_command_counter = scene.draw_command_count_buffer_address(),
        .generated_draw_commands_buffer_address
        = scene.generated_draw_commands_buffer_address(),
        .draw_command_instance_offsets
        = scene.draw_command_instance_offsets_buffer_address(),
    };
    const vk::PushConstantsInfo push_constants_info{
        .layout     = m_draw_command_generation_pipeline_layout,
        .stageFlags = vk::ShaderStageFlagBits::eCompute,
        .size       = sizeof(decltype(push_constants)),
        .pValues    = &push_constants,
    };
    command_buffer.record_push_constants(push_constants_info);

    const std::array dispatch_buffer_memory_barriers{
        vk::BufferMemoryBarrier2{
                                 .srcStageMask  = vk::PipelineStageFlagBits2::eComputeShader,
                                 .srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
                                 .dstStageMask  = vk::PipelineStageFlagBits2::eComputeShader,
                                 .dstAccessMask = vk::AccessFlagBits2::eShaderRead,
                                 .buffer = scene.draw_command_instance_counts_buffer_region().buffer().get(),
                                 .offset = scene.draw_command_instance_counts_buffer_region().offset(),
                                 .size   = scene.draw_command_instance_counts_buffer_region().size(),
                                 },
        vk::BufferMemoryBarrier2{
                                 .srcStageMask  = vk::PipelineStageFlagBits2::eTransfer,
                                 .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
                                 .dstStageMask  = vk::PipelineStageFlagBits2::eComputeShader,
                                 .dstAccessMask = vk::AccessFlagBits2::eShaderWrite,
                                 .buffer        = scene.instance_counter_buffer_region().buffer().get(),
                                 .offset        = scene.instance_counter_buffer_region().offset(),
                                 .size          = scene.instance_counter_buffer_region().size(),
                                 },
        vk::BufferMemoryBarrier2{
                                 .srcStageMask  = vk::PipelineStageFlagBits2::eTransfer,
                                 .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
                                 .dstStageMask  = vk::PipelineStageFlagBits2::eComputeShader,
                                 .dstAccessMask = vk::AccessFlagBits2::eShaderWrite,
                                 .buffer        = scene.draw_command_count_buffer_region().buffer().get(),
                                 .offset        = scene.draw_command_count_buffer_region().offset(),
                                 .size          = scene.draw_command_count_buffer_region().size(),
                                 },
        vk::BufferMemoryBarrier2{
                                 .srcStageMask  = vk::PipelineStageFlagBits2::eDrawIndirect,
                                 .srcAccessMask = vk::AccessFlagBits2::eIndirectCommandRead,
                                 .dstStageMask  = vk::PipelineStageFlagBits2::eComputeShader,
                                 .dstAccessMask = vk::AccessFlagBits2::eShaderWrite,
                                 .buffer        = scene.generated_draw_commands_buffer_region().buffer().get(),
                                 .offset        = scene.generated_draw_commands_buffer_region().offset(),
                                 .size          = scene.generated_draw_commands_buffer_region().size(),
                                 },
        vk::BufferMemoryBarrier2{
                                 .srcStageMask  = vk::PipelineStageFlagBits2::eComputeShader,
                                 .srcAccessMask = vk::AccessFlagBits2::eShaderRead,
                                 .dstStageMask  = vk::PipelineStageFlagBits2::eComputeShader,
                                 .dstAccessMask = vk::AccessFlagBits2::eShaderWrite,
                                 .buffer = scene.draw_command_instance_offsets_buffer_region().buffer().get(),
                                 .offset = scene.draw_command_instance_offsets_buffer_region().offset(),
                                 .size   = scene.draw_command_instance_offsets_buffer_region().size(),
                                 },
    };
    const kiln::gfx::renderer::DependencyInfo dispatch_dependency_info{
        .buffer_memory_barriers = dispatch_buffer_memory_barriers,
    };
    command_buffer.record_barrier(dispatch_dependency_info);

    command_buffer.record_dispatch((scene.max_draw_command_count() + 255) / 256, 1, 1);


    command_buffer.end_recording();


    const kiln::gfx::renderer::SubmitInfo submit_info{ &transient_memory_resource };
    queue_provider.graphics_queue_as<kiln::gfx::renderer::ComputeQueueRef>()
        ->submit(command_buffer, submit_info);
}

auto Renderer::generate_instance_indices(
    kiln::gfx::renderer::QueueProvider& queue_provider,
    const Scene&                        scene,
    std::pmr::memory_resource&          transient_memory_resource
) -> void
{
    if (scene.max_instance_count() == 0)
    {
        return;
    }

    kiln::gfx::renderer::ComputeCommandBuffer& command_buffer{
        m_instance_index_generation_command_buffers[m_current_frame_index]
    };


    command_buffer.begin_recording();


    command_buffer.record_pipeline_bind(m_instance_index_generation_pipeline);

    const shaders::InstanceIndexGenerationPushConstants push_constants{
        .global_instance_count = scene.max_instance_count(),
        .instance_draw_command_offsets
        = scene.instance_draw_command_offsets_buffer_address(),
        .instance_draw_command_indices
        = scene.instance_draw_command_indices_buffer_address(),
        .draw_command_instance_offsets
        = scene.draw_command_instance_offsets_buffer_address(),
        .instance_indices = scene.instance_indices_buffer_address(),
    };
    const vk::PushConstantsInfo push_constants_info{
        .layout     = m_instance_index_generation_pipeline_layout,
        .stageFlags = vk::ShaderStageFlagBits::eCompute,
        .size       = sizeof(decltype(push_constants)),
        .pValues    = &push_constants,
    };
    command_buffer.record_push_constants(push_constants_info);

    const std::array dispatch_buffer_memory_barriers{
        vk::BufferMemoryBarrier2{
                                 .srcStageMask  = vk::PipelineStageFlagBits2::eComputeShader,
                                 .srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
                                 .dstStageMask  = vk::PipelineStageFlagBits2::eComputeShader,
                                 .dstAccessMask = vk::AccessFlagBits2::eShaderRead,
                                 .buffer = scene.instance_draw_command_offsets_buffer_region().buffer().get(),
                                 .offset = scene.instance_draw_command_offsets_buffer_region().offset(),
                                 .size   = scene.instance_draw_command_offsets_buffer_region().size(),
                                 },
        vk::BufferMemoryBarrier2{
                                 .srcStageMask  = vk::PipelineStageFlagBits2::eComputeShader,
                                 .srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
                                 .dstStageMask  = vk::PipelineStageFlagBits2::eComputeShader,
                                 .dstAccessMask = vk::AccessFlagBits2::eShaderRead,
                                 .buffer = scene.draw_command_instance_offsets_buffer_region().buffer().get(),
                                 .offset = scene.draw_command_instance_offsets_buffer_region().offset(),
                                 .size   = scene.draw_command_instance_offsets_buffer_region().size(),
                                 },
        vk::BufferMemoryBarrier2{
                                 .srcStageMask  = vk::PipelineStageFlagBits2::eVertexShader,
                                 .srcAccessMask = vk::AccessFlagBits2::eShaderRead,
                                 .dstStageMask  = vk::PipelineStageFlagBits2::eComputeShader,
                                 .dstAccessMask = vk::AccessFlagBits2::eShaderWrite,
                                 .buffer        = scene.instance_indices_buffer_region().buffer().get(),
                                 .offset        = scene.instance_indices_buffer_region().offset(),
                                 .size          = scene.instance_indices_buffer_region().size(),
                                 },
    };
    const kiln::gfx::renderer::DependencyInfo dispatch_dependency_info{
        .buffer_memory_barriers = dispatch_buffer_memory_barriers,
    };
    command_buffer.record_barrier(dispatch_dependency_info);

    command_buffer.record_dispatch((scene.max_instance_count() + 255) / 256, 1, 1);


    command_buffer.end_recording();


    const kiln::gfx::renderer::SubmitInfo submit_info{ &transient_memory_resource };
    queue_provider.graphics_queue_as<kiln::gfx::renderer::ComputeQueueRef>()
        ->submit(command_buffer, submit_info);
}

auto Renderer::draw(
    kiln::gfx::renderer::QueueProvider&       queue_provider,
    const kiln::gfx::renderer::RenderSurface& surface,
    const Scene&                              scene,
    const Camera&                             camera,
    const uint32_t                            swapchain_image_index,
    std::pmr::memory_resource&                transient_memory_resource
) -> void
{
    kiln::gfx::renderer::GraphicsCommandBuffer& command_buffer{
        m_graphics_command_buffers[m_current_frame_index]
    };

    command_buffer.begin_recording();

    const std::array render_buffer_memory_barriers{
        vk::BufferMemoryBarrier2{
                                 .srcStageMask  = vk::PipelineStageFlagBits2::eComputeShader,
                                 .srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
                                 .dstStageMask  = vk::PipelineStageFlagBits2::eVertexShader,
                                 .dstAccessMask = vk::AccessFlagBits2::eShaderRead,
                                 .buffer        = scene.instance_indices_buffer_region().buffer().get(),
                                 .offset        = scene.instance_indices_buffer_region().offset(),
                                 .size          = scene.instance_indices_buffer_region().size(),
                                 },
        vk::BufferMemoryBarrier2{
                                 .srcStageMask  = vk::PipelineStageFlagBits2::eComputeShader,
                                 .srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
                                 .dstStageMask  = vk::PipelineStageFlagBits2::eDrawIndirect,
                                 .dstAccessMask = vk::AccessFlagBits2::eIndirectCommandRead,
                                 .buffer        = scene.draw_command_count_buffer_region().buffer().get(),
                                 .offset        = scene.draw_command_count_buffer_region().offset(),
                                 .size          = scene.draw_command_count_buffer_region().size(),
                                 },
        vk::BufferMemoryBarrier2{
                                 .srcStageMask  = vk::PipelineStageFlagBits2::eComputeShader,
                                 .srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
                                 .dstStageMask  = vk::PipelineStageFlagBits2::eDrawIndirect,
                                 .dstAccessMask = vk::AccessFlagBits2::eIndirectCommandRead,
                                 .buffer        = scene.generated_draw_commands_buffer_region().buffer().get(),
                                 .offset        = scene.generated_draw_commands_buffer_region().offset(),
                                 .size          = scene.generated_draw_commands_buffer_region().size(),
                                 },
    };
    const std::array render_image_memory_barriers{
        vk::ImageMemoryBarrier2{
            .srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
            .oldLayout = vk::ImageLayout::eUndefined,
            .newLayout = vk::ImageLayout::eAttachmentOptimal,
            .image = surface.image_at(swapchain_image_index),
            .subresourceRange = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .levelCount = 1,
                .layerCount = 1,
              },
        },
        vk::ImageMemoryBarrier2{
            .srcStageMask = vk::PipelineStageFlagBits2::eLateFragmentTests,
            .srcAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
            .dstStageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests,
            .dstAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
            .oldLayout = vk::ImageLayout::eUndefined,
            .newLayout = vk::ImageLayout::eAttachmentOptimal,
            .image = m_depth_image.get(),
            .subresourceRange = {
                .aspectMask = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil,
                .levelCount = 1,
                .layerCount = 1,
              },
        },
    };
    const kiln::gfx::renderer::DependencyInfo render_dependency_info{
        .buffer_memory_barriers = render_buffer_memory_barriers,
        .image_memory_barriers  = render_image_memory_barriers,
    };
    command_buffer.record_barrier(render_dependency_info);

    const vk::Rect2D render_area{
        .extent = *surface.extent(),
    };
    const kiln::gfx::renderer::RenderPass render_pass{
        render_area,
        std::array{
            kiln::gfx::renderer::ColorAttachment{
                surface.image_view_at(swapchain_image_index),
            }
                .set_clear_value(std::array{ 0.01f, 0.01f, 0.01f, 1.f })   //
        },
        kiln::gfx::renderer::DepthAttachment{ m_depth_image_view }.set_clear_value(1)
    };
    command_buffer.record_render_pass_start(render_pass);


    if (scene.max_draw_command_count() != 0)
    {
        command_buffer.record_pipeline_bind(m_graphics_pipeline);

        const shaders::Scene shader_scene{
            .geometry_buffer_address  = scene.geometry_buffer_address(),
            .materials_buffer_address = scene.materials_buffer_address(),
            .instance_buffer_address  = scene.instance_buffer_address(),
            .instance_indices_buffer_address
            = m_disable_culling ? vk::DeviceAddress{}
                                : scene.instance_indices_buffer_address(),
            .draw_command_buffer_address = scene.generated_draw_commands_buffer_address(),
        };
        const vk::PushConstantsInfo scene_push_constants_info{
            .layout     = m_graphics_pipeline_layout,
            .stageFlags = vk::ShaderStageFlagBits::eVertex,
            .size       = sizeof(decltype(shader_scene)),
            .pValues    = &shader_scene,
        };
        const shaders::Camera shader_camera{
            .position = glm::vec4{ camera.position(), 1.0 },
            .view_projection_matrix
            = glm::perspectiveFovRH_ZO(
                  camera.fov(),
                  static_cast<double>(surface.extent()->width),
                  static_cast<double>(surface.extent()->height),
                  camera.near_plane(),
                  camera.far_plane()
              )
            * glm::mat4_cast(glm::conjugate(camera.orientation()))
            * glm::translate(glm::identity<glm::dmat4>(), -camera.position()),
        };
        const vk::PushConstantsInfo camera_push_constants_info{
            .layout     = m_graphics_pipeline_layout,
            .stageFlags = vk::ShaderStageFlagBits::eVertex,
            .offset     = sizeof(shaders::Scene),
            .size       = sizeof(decltype(shader_camera)),
            .pValues    = &shader_camera,
        };
        command_buffer.record_push_constants(scene_push_constants_info);
        command_buffer.record_push_constants(camera_push_constants_info);

        command_buffer.record_indirect_draw_count(
            scene.generated_draw_commands_buffer_region(),
            scene.draw_command_count_buffer_region(),
            scene.max_draw_command_count(),
            sizeof(shaders::DrawCommand)
        );
    }


    command_buffer.record_render_pass_finish();

    const vk::ImageMemoryBarrier2 present_image_memory_barrier {
        .srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        .srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
        .dstStageMask = vk::PipelineStageFlagBits2::eAllCommands,
        .oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .newLayout = vk::ImageLayout::ePresentSrcKHR,
        .image = surface.image_at(swapchain_image_index),
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
    command_buffer.record_barrier(present_dependency_info);

    command_buffer.end_recording();


    const vk::SemaphoreSubmitInfo image_acquired_semaphore_info{
        .semaphore = m_image_acquired_semaphores[m_current_frame_index],
        .stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
    };
    const vk::SemaphoreSubmitInfo render_finished_semaphore_info{
        .semaphore = m_render_finished_semaphores[swapchain_image_index],
        .stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
    };
    kiln::gfx::renderer::SubmitInfo submit_info{ &transient_memory_resource };
    submit_info.wait_semaphores().push_back(image_acquired_semaphore_info);
    submit_info.signal_semaphores().push_back(render_finished_semaphore_info);
    submit_info.fence() = *m_render_finished_fences[m_current_frame_index];
    queue_provider.graphics_queue()->submit(command_buffer, submit_info);
}

}   // namespace demo
