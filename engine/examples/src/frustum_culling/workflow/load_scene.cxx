module;

#include <algorithm>
#include <filesystem>
#include <memory_resource>
#include <print>
#include <ranges>
#include <span>

#include <vk_mem_alloc.h>

#include <fastgltf/types.hpp>

#include <kiln/util/contract_macros.hpp>
#include <kiln/util/memory/lifetimebound.hpp>

module examples.frustum_culling.workflow.load_scene;

import vulkan;

import kiln.gfx.renderer.memory.Allocator;
import kiln.gfx.renderer.memory.Buffer;
import kiln.gfx.renderer.memory.BufferRegion;
import kiln.gfx.renderer.memory.LazyCopy;
import kiln.gfx.renderer.stream.StagingRequest;
import kiln.gfx.renderer.stream.StagingStream;
import kiln.util.contracts;
import kiln.util.Lazy;

import examples.frustum_culling.shaders;
import examples.frustum_culling.workflow.GltfModelLoader;
import examples.frustum_culling.workflow.ModelDescription;

namespace demo {

template <typename T>
[[nodiscard]]
constexpr T align_up(const T value, const T alignment)
{
    PRECOND(std::has_single_bit(alignment));
    return value + (alignment - 1) & ~(alignment - 1);
}

template <typename ProjectBufferSize_T, typename ProjectBufferAlignment_T>
[[nodiscard]]
auto buffer_size_from(
    const std::span<const GltfModelLoader> model_loaders,
    ProjectBufferSize_T&&                  project_buffer_size,
    ProjectBufferAlignment_T&&             project_buffer_alignment
) -> uint32_t
{
    uint32_t result{};

    for (const GltfModelLoader& model_loader : model_loaders)
    {
        result = align_up(result, std::invoke(project_buffer_alignment));
        result += std::invoke(project_buffer_size, model_loader);
    }

    return result;
}

[[nodiscard]]
auto create_buffer(
    const kiln::gfx::renderer::Device& device,
    kiln::gfx::renderer::Allocator&    allocator,
    const vk::BufferUsageFlags2        usage_flags,
    const uint32_t                     size,
    const uint32_t                     alignment,
    const char*                        name = nullptr
) -> kiln::gfx::renderer::Buffer
{
    const vk::BufferUsageFlags2CreateInfo buffer_usage_flags{
        .usage = usage_flags,
    };
    const vk::BufferCreateInfo buffer_create_info{
        .pNext       = &buffer_usage_flags,
        .size        = size,
        .sharingMode = vk::SharingMode::eExclusive,
    };
    constexpr static VmaAllocationCreateInfo allocation_create_info{
        .usage = VMA_MEMORY_USAGE_AUTO,
    };
    kiln::gfx::renderer::Buffer result{
        allocator.create_buffer(buffer_create_info, allocation_create_info, alignment)
    };

    if (name != nullptr && result.get() != nullptr)
    {
        const vk::DebugUtilsObjectNameInfoEXT name_info{
            .objectType = vk::ObjectType::eBuffer,
            .objectHandle
            = reinterpret_cast<uint64_t>(static_cast<VkBuffer>(result.get())),
            .pObjectName = name,
        };
        device.logical_device().setDebugUtilsObjectNameEXT(name_info);
    }

    return result;
}

[[nodiscard]]
auto create_instance_draw_command_indices_buffer(
    const kiln::gfx::renderer::Device&     device,
    kiln::gfx::renderer::Allocator&        allocator,
    const std::span<const GltfModelLoader> model_loaders
) -> kiln::gfx::renderer::Buffer
{
    return create_buffer(
        device,
        allocator,
        vk::BufferUsageFlagBits2::eTransferDst
            | vk::BufferUsageFlagBits2::eShaderDeviceAddress,
        buffer_size_from(
            model_loaders,
            [](const GltfModelLoader& loader) -> uint32_t
            {
                return loader.instance_count() * sizeof(uint32_t);   //
            },
            std::integral_constant<uint32_t, alignof(uint32_t)>{}
        ),
        alignof(uint32_t),
        "Buffer__instance_draw_command_indices"
    );
}

[[nodiscard]]
auto create_instance_sphere_bounding_volumes_buffer(
    const kiln::gfx::renderer::Device&     device,
    kiln::gfx::renderer::Allocator&        allocator,
    const std::span<const GltfModelLoader> model_loaders
) -> kiln::gfx::renderer::Buffer
{
    return create_buffer(
        device,
        allocator,
        vk::BufferUsageFlagBits2::eTransferDst
            | vk::BufferUsageFlagBits2::eShaderDeviceAddress,
        buffer_size_from(
            model_loaders,
            [](const GltfModelLoader& loader) -> uint32_t
            {
                return loader.instance_count() * sizeof(shaders::SBV);   //
            },
            std::integral_constant<uint32_t, alignof(shaders::SBV)>{}
        ),
        alignof(shaders::SBV),
        "Buffer__instance_sphere_bounding_volumes"
    );
}

[[nodiscard]]
auto create_draw_command_instance_counts_buffer(
    const kiln::gfx::renderer::Device&     device,
    kiln::gfx::renderer::Allocator&        allocator,
    const std::span<const GltfModelLoader> model_loaders
) -> kiln::gfx::renderer::Buffer
{
    return create_buffer(
        device,
        allocator,
        vk::BufferUsageFlagBits2::eTransferDst
            | vk::BufferUsageFlagBits2::eShaderDeviceAddress,
        buffer_size_from(
            model_loaders,
            [](const GltfModelLoader& loader) -> uint32_t
            {
                return loader.draw_command_count() * sizeof(uint32_t);   //
            },
            std::integral_constant<uint32_t, alignof(uint32_t)>{}
        ),
        alignof(uint32_t),
        "Buffer__draw_command_instance_counts"
    );
}

[[nodiscard]]
auto create_instance_draw_command_offsets_buffer(
    const kiln::gfx::renderer::Device&     device,
    kiln::gfx::renderer::Allocator&        allocator,
    const std::span<const GltfModelLoader> model_loaders
) -> kiln::gfx::renderer::Buffer
{
    return create_buffer(
        device,
        allocator,
        vk::BufferUsageFlagBits2::eShaderDeviceAddress,
        buffer_size_from(
            model_loaders,
            [](const GltfModelLoader& loader) -> uint32_t
            {
                return loader.instance_count() * sizeof(uint32_t);   //
            },
            std::integral_constant<uint32_t, alignof(uint32_t)>{}
        ),
        alignof(uint32_t),
        "Buffer__instance_draw_command_offsets"
    );
}

[[nodiscard]]
auto create_original_draw_commands_buffer(
    const kiln::gfx::renderer::Device& device,
    kiln::gfx::renderer::Allocator&    allocator,
    const uint32_t                     draw_command_count
) -> kiln::gfx::renderer::Buffer
{
    return create_buffer(
        device,
        allocator,
        vk::BufferUsageFlagBits2::eTransferDst
            | vk::BufferUsageFlagBits2::eShaderDeviceAddress,
        sizeof(shaders::DrawCommand) * draw_command_count,
        alignof(shaders::DrawCommand),
        "Buffer__original_draw_commands"
    );
}

[[nodiscard]]
auto create_instance_counter_buffer(
    const kiln::gfx::renderer::Device& device,
    kiln::gfx::renderer::Allocator&    allocator
) -> kiln::gfx::renderer::Buffer
{
    return create_buffer(
        device,
        allocator,
        vk::BufferUsageFlagBits2::eTransferDst
            | vk::BufferUsageFlagBits2::eShaderDeviceAddress,
        sizeof(uint32_t),
        alignof(uint32_t),
        "Buffer__instance_counter"
    );
}

[[nodiscard]]
auto create_generated_draw_commands_buffer(
    const kiln::gfx::renderer::Device& device,
    kiln::gfx::renderer::Allocator&    allocator,
    const uint32_t                     max_draw_command_count
) -> kiln::gfx::renderer::Buffer
{
    return create_buffer(
        device,
        allocator,
        vk::BufferUsageFlagBits2::eTransferDst
            | vk::BufferUsageFlagBits2::eIndirectBuffer
            | vk::BufferUsageFlagBits2::eShaderDeviceAddress,
        align_up(sizeof(uint32_t), alignof(shaders::DrawCommand))
            + sizeof(shaders::DrawCommand) * max_draw_command_count,
        std::max(alignof(uint32_t), alignof(shaders::DrawCommand)),
        "Buffer__generated_draw_commands"
    );
}

[[nodiscard]]
auto create_draw_command_instance_offsets_buffer(
    const kiln::gfx::renderer::Device& device,
    kiln::gfx::renderer::Allocator&    allocator,
    const uint32_t                     draw_command_count
) -> kiln::gfx::renderer::Buffer
{
    return create_buffer(
        device,
        allocator,
        vk::BufferUsageFlagBits2::eShaderDeviceAddress,
        sizeof(uint32_t) * draw_command_count,
        alignof(uint32_t),
        "Buffer__draw_command_instance_offsets"
    );
}

[[nodiscard]]
auto create_instance_indices_buffer(
    const kiln::gfx::renderer::Device& device,
    kiln::gfx::renderer::Allocator&    allocator,
    const uint32_t                     instance_count
) -> kiln::gfx::renderer::Buffer
{
    return create_buffer(
        device,
        allocator,
        vk::BufferUsageFlagBits2::eShaderDeviceAddress,
        sizeof(uint32_t) * instance_count,
        alignof(uint32_t),
        "Buffer__instance_indices"
    );
}

[[nodiscard]]
auto create_geometry_buffer(
    const kiln::gfx::renderer::Device&     device,
    kiln::gfx::renderer::Allocator&        allocator,
    const std::span<const GltfModelLoader> model_loaders
) -> kiln::gfx::renderer::Buffer
{
    return create_buffer(
        device,
        allocator,
        vk::BufferUsageFlagBits2::eTransferDst
            | vk::BufferUsageFlagBits2::eShaderDeviceAddress,
        buffer_size_from(
            model_loaders,
            &GltfModelLoader::geometry_buffer_size_bytes,
            GltfModelLoader::geometry_buffer_alignment
        ),
        GltfModelLoader::geometry_buffer_alignment(),
        "Buffer__geometry"
    );
}

[[nodiscard]]
auto create_materials_buffer(
    const kiln::gfx::renderer::Device&     device,
    kiln::gfx::renderer::Allocator&        allocator,
    const std::span<const GltfModelLoader> model_loaders
) -> kiln::gfx::renderer::Buffer
{
    return create_buffer(
        device,
        allocator,
        vk::BufferUsageFlagBits2::eTransferDst
            | vk::BufferUsageFlagBits2::eShaderDeviceAddress,
        buffer_size_from(
            model_loaders,
            &GltfModelLoader::materials_buffer_size_bytes,
            GltfModelLoader::materials_buffer_alignment
        ),
        GltfModelLoader::materials_buffer_alignment(),
        "Buffer__materials"
    );
}

[[nodiscard]]
auto create_instance_buffer(
    const kiln::gfx::renderer::Device&     device,
    kiln::gfx::renderer::Allocator&        allocator,
    const std::span<const GltfModelLoader> model_loaders
) -> kiln::gfx::renderer::Buffer
{
    return create_buffer(
        device,
        allocator,
        vk::BufferUsageFlagBits2::eTransferDst
            | vk::BufferUsageFlagBits2::eShaderDeviceAddress,
        buffer_size_from(
            model_loaders,
            &GltfModelLoader::instance_buffer_size_bytes,
            GltfModelLoader::instance_buffer_alignment
        ),
        GltfModelLoader::instance_buffer_alignment(),
        "Buffer__instance"
    );
}

auto stage_instance_draw_command_indices_buffer(
    kiln::gfx::renderer::Allocator&     allocator,
    kiln::gfx::renderer::StagingStream& staging_stream,
    const std::span<GltfModelLoader>    model_loaders,
    kiln::gfx::renderer::Buffer&        instance_draw_command_indices_buffer
) -> void
{
    const vk::MemoryPropertyFlags buffer_memory_properties{
        instance_draw_command_indices_buffer.allocation().memory_properties()
    };

    uint32_t buffer_byte_offset{};
    uint32_t instance_draw_command_offset{};
    for (GltfModelLoader& model_loader : model_loaders)
    {
        if (model_loader.instance_count() != 0)
        {
            buffer_byte_offset
                = align_up(buffer_byte_offset, static_cast<uint32_t>(alignof(uint32_t)));

            model_loader.set_instance_draw_command_index_offset(
                instance_draw_command_offset
            );

            const kiln::gfx::renderer::BufferRegion destination{
                instance_draw_command_indices_buffer,
                buffer_byte_offset,
                model_loader.instance_count() * sizeof(uint32_t),
            };
            if (buffer_memory_properties & vk::MemoryPropertyFlagBits::eHostVisible)
            {
                allocator.host_copy(
                    model_loader.instance_draw_command_index_writer(),
                    destination
                );
            }
            else
            {
                staging_stream.record(
                    kiln::gfx::renderer::StagingRequest{
                        model_loader.instance_draw_command_index_writer(),
                        destination,
                    }
                );
            }

            buffer_byte_offset += destination.size();
            instance_draw_command_offset += model_loader.draw_command_count();
        }
    }

    if (buffer_memory_properties & vk::MemoryPropertyFlagBits::eHostVisible)
    {
        std::println(
            "Copied {} bytes (instance draw command indices) from host to gpu",
            instance_draw_command_indices_buffer.size()
        );
    }
    else
    {
        std::println(
            "Staged {} bytes (instance draw command indices) from host to gpu",
            instance_draw_command_indices_buffer.size()
        );
    }
}

auto stage_instance_sphere_bounding_volumes_buffer(
    kiln::gfx::renderer::Allocator&        allocator,
    kiln::gfx::renderer::StagingStream&    staging_stream,
    const std::span<const GltfModelLoader> model_loaders,
    kiln::gfx::renderer::Buffer&           instance_sphere_bounding_volumes_buffer
) -> void
{
    const vk::MemoryPropertyFlags buffer_memory_properties{
        instance_sphere_bounding_volumes_buffer.allocation().memory_properties()
    };

    uint32_t buffer_byte_offset{};
    for (const GltfModelLoader& model_loader : model_loaders)
    {
        if (model_loader.instance_count() != 0)
        {
            buffer_byte_offset = align_up(
                buffer_byte_offset,
                static_cast<uint32_t>(alignof(shaders::SBV))
            );

            const kiln::gfx::renderer::BufferRegion destination{
                instance_sphere_bounding_volumes_buffer,
                buffer_byte_offset,
                model_loader.instance_count() * sizeof(shaders::SBV),
            };
            if (buffer_memory_properties & vk::MemoryPropertyFlagBits::eHostVisible)
            {
                allocator.host_copy(
                    model_loader.instance_sphere_bounding_volume_writer(),
                    destination
                );
            }
            else
            {
                staging_stream.record(
                    kiln::gfx::renderer::StagingRequest{
                        model_loader.instance_sphere_bounding_volume_writer(),
                        destination,
                    }
                );
            }

            buffer_byte_offset += destination.size();
        }
    }

    if (buffer_memory_properties & vk::MemoryPropertyFlagBits::eHostVisible)
    {
        std::println(
            "Copied {} bytes (instance sphere bounding volumes) from host to gpu",
            instance_sphere_bounding_volumes_buffer.size()
        );
    }
    else
    {
        std::println(
            "Staged {} bytes (instance sphere bounding volumes) from host to gpu",
            instance_sphere_bounding_volumes_buffer.size()
        );
    }
}

auto stage_geometry_buffer(
    kiln::gfx::renderer::Allocator&     allocator,
    kiln::gfx::renderer::StagingStream& staging_stream,
    const std::span<GltfModelLoader>    model_loaders,
    kiln::gfx::renderer::Buffer&        geometry_buffer
) -> void
{
    const vk::MemoryPropertyFlags buffer_memory_properties{
        geometry_buffer.allocation().memory_properties()
    };

    uint32_t buffer_byte_offset{};
    for (GltfModelLoader& model_loader : model_loaders)
    {
        if (model_loader.geometry_buffer_size_bytes() != 0)
        {
            buffer_byte_offset = align_up(
                buffer_byte_offset,
                GltfModelLoader::geometry_buffer_alignment()
            );

            model_loader.set_geometry_buffer_byte_offset(buffer_byte_offset);

            const kiln::gfx::renderer::BufferRegion destination{
                geometry_buffer,
                buffer_byte_offset,
                model_loader.geometry_buffer_size_bytes(),
            };
            if (buffer_memory_properties & vk::MemoryPropertyFlagBits::eHostVisible)
            {
                allocator.host_copy(model_loader.geometry_writer(), destination);
            }
            else
            {
                staging_stream.record(
                    kiln::gfx::renderer::StagingRequest{
                        model_loader.geometry_writer(),
                        destination,
                    }
                );
            }

            buffer_byte_offset += destination.size();
        }
    }

    if (buffer_memory_properties & vk::MemoryPropertyFlagBits::eHostVisible)
    {
        std::println(
            "Copied {} bytes (geometry) from host to gpu",
            geometry_buffer.size()
        );
    }
    else
    {
        std::println(
            "Staged {} bytes (geometry) from host to gpu",
            geometry_buffer.size()
        );
    }
}

auto stage_materials_buffer(
    kiln::gfx::renderer::Allocator&     allocator,
    kiln::gfx::renderer::StagingStream& staging_stream,
    const std::span<GltfModelLoader>    model_loaders,
    kiln::gfx::renderer::Buffer&        materials_buffer
) -> void
{
    const vk::MemoryPropertyFlags buffer_memory_properties{
        materials_buffer.allocation().memory_properties()
    };

    uint32_t buffer_byte_offset{};
    for (GltfModelLoader& model_loader : model_loaders)
    {
        if (model_loader.materials_buffer_size_bytes() != 0)
        {
            buffer_byte_offset = align_up(
                buffer_byte_offset,
                GltfModelLoader::materials_buffer_alignment()
            );

            model_loader.set_materials_buffer_byte_offset(buffer_byte_offset);

            const kiln::gfx::renderer::BufferRegion destination{
                materials_buffer,
                buffer_byte_offset,
                model_loader.materials_buffer_size_bytes(),
            };
            if (buffer_memory_properties & vk::MemoryPropertyFlagBits::eHostVisible)
            {
                allocator.host_copy(model_loader.material_writer(), destination);
            }
            else
            {
                staging_stream.record(
                    kiln::gfx::renderer::StagingRequest{
                        model_loader.material_writer(),
                        destination,
                    }
                );
            }

            buffer_byte_offset += destination.size();
        }
    }

    if (buffer_memory_properties & vk::MemoryPropertyFlagBits::eHostVisible)
    {
        std::println(
            "Copied {} bytes (materials) from host to gpu",
            materials_buffer.size()
        );
    }
    else
    {
        std::println(
            "Staged {} bytes (materials) from host to gpu",
            materials_buffer.size()
        );
    }
}

auto stage_instance_buffer(
    kiln::gfx::renderer::Allocator&     allocator,
    kiln::gfx::renderer::StagingStream& staging_stream,
    const std::span<GltfModelLoader>    model_loaders,
    kiln::gfx::renderer::Buffer&        instance_buffer
) -> void
{
    const vk::MemoryPropertyFlags buffer_memory_properties{
        instance_buffer.allocation().memory_properties()
    };

    uint32_t buffer_byte_offset{};
    for (GltfModelLoader& model_loader : model_loaders)
    {
        if (model_loader.instance_buffer_size_bytes() != 0)
        {
            buffer_byte_offset = align_up(
                buffer_byte_offset,
                GltfModelLoader::instance_buffer_alignment()
            );

            model_loader.set_instance_buffer_byte_offset(buffer_byte_offset);

            const kiln::gfx::renderer::BufferRegion destination{
                instance_buffer,
                buffer_byte_offset,
                model_loader.instance_buffer_size_bytes(),
            };
            if (buffer_memory_properties & vk::MemoryPropertyFlagBits::eHostVisible)
            {
                allocator.host_copy(model_loader.instance_writer(), destination);
            }
            else
            {
                staging_stream.record(
                    kiln::gfx::renderer::StagingRequest{
                        model_loader.instance_writer(),
                        destination,
                    }
                );
            }

            buffer_byte_offset += destination.size();
        }
    }

    if (buffer_memory_properties & vk::MemoryPropertyFlagBits::eHostVisible)
    {
        std::println(
            "Copied {} bytes (instances) from host to gpu",
            instance_buffer.size()
        );
    }
    else
    {
        std::println(
            "Staged {} bytes (instances) from host to gpu",
            instance_buffer.size()
        );
    }
}

[[nodiscard]]
auto draw_count_from(const std::span<const GltfModelLoader> model_loaders) -> uint32_t
{
    uint32_t result{};

    for (const GltfModelLoader& model_loader : model_loaders)
    {
        result += model_loader.draw_command_count();
    }

    return result;
}

[[nodiscard]]
auto lazy_copy_draw_command_count(const std::span<const GltfModelLoader> model_loaders)
    -> kiln::gfx::renderer::LazyCopy
{
    return kiln::gfx::renderer::LazyCopy{
        [draw_count = draw_count_from(model_loaders)](
            const std::span<std::byte> staging_buffer
        ) -> void
        {
            assert(staging_buffer.size() == sizeof(decltype(draw_count)));
            std::memcpy(staging_buffer.data(), &draw_count, sizeof(decltype(draw_count)));
        }
    };
}

// clang-format off
[[nodiscard]]
auto lazy_copy_draw_commands(
    kiln_lifetimebound const std::span<GltfModelLoader> model_loaders
) -> kiln::gfx::renderer::LazyCopy
// clang-format on
{
    return kiln::gfx::renderer::LazyCopy{
        [draw_count = draw_count_from(model_loaders),
         model_loaders]   //
        (const std::span<std::byte> staging_buffer) -> void
        {
            uint32_t buffer_byte_offset{ 0 };
            uint32_t instance_index_offset{};
            for (GltfModelLoader& model_loader : model_loaders)
            {
                const uint32_t draw_commands_size_bytes{
                    static_cast<uint32_t>(
                        model_loader.draw_command_count() * sizeof(shaders::DrawCommand)
                    ),
                };
                model_loader.set_instance_index_offset(instance_index_offset);
                model_loader.draw_command_writer()(
                    staging_buffer.subspan(buffer_byte_offset, draw_commands_size_bytes)
                );
                buffer_byte_offset += draw_commands_size_bytes;
                instance_index_offset += model_loader.instance_count();
            }
        }
    };
}

auto stage_draw_command_count(
    kiln::gfx::renderer::Allocator&          allocator,
    kiln::gfx::renderer::StagingStream&      staging_stream,
    const std::span<const GltfModelLoader>   model_loaders,
    const kiln::gfx::renderer::BufferRegion& draw_command_count_buffer
) -> void
{
    if (draw_command_count_buffer.allocation().memory_properties()
        & vk::MemoryPropertyFlagBits::eHostVisible)
    {
        allocator.host_copy(
            lazy_copy_draw_command_count(model_loaders),
            draw_command_count_buffer
        );
        std::println(
            "Copied {} bytes (draw command count) from host to gpu",
            draw_command_count_buffer.size()
        );
    }
    else
    {
        staging_stream.record(
            kiln::gfx::renderer::StagingRequest{
                lazy_copy_draw_command_count(model_loaders),
                draw_command_count_buffer,
            }
        );
        std::println(
            "Staged {} bytes (draw command count) from host to gpu",
            draw_command_count_buffer.size()
        );
    }
}

auto stage_draw_commands(
    kiln::gfx::renderer::Allocator&          allocator,
    kiln::gfx::renderer::StagingStream&      staging_stream,
    const std::span<GltfModelLoader>         model_loaders,
    const kiln::gfx::renderer::BufferRegion& draw_command_buffer
) -> void
{
    if (draw_command_buffer.allocation().memory_properties()
        & vk::MemoryPropertyFlagBits::eHostVisible)
    {
        allocator.host_copy(lazy_copy_draw_commands(model_loaders), draw_command_buffer);
        std::println(
            "Copied {} bytes (draw commands) from host to gpu",
            draw_command_buffer.size()
        );
    }
    else
    {
        staging_stream.record(
            kiln::gfx::renderer::StagingRequest{
                lazy_copy_draw_commands(model_loaders),
                draw_command_buffer,
            }
        );
        std::println(
            "Staged {} bytes (draw commands) from host to gpu",
            draw_command_buffer.size()
        );
    }
}

auto stage_models(
    kiln::gfx::renderer::Allocator&     allocator,
    kiln::gfx::renderer::StagingStream& staging_stream,
    const std::span<GltfModelLoader>    model_loaders,
    const bool                          disable_culling,
    kiln::gfx::renderer::Buffer&        instance_draw_command_indices_buffer,
    kiln::gfx::renderer::Buffer&        instance_sphere_bounding_volumes_buffer,
    kiln::gfx::renderer::Buffer&        geometry_buffer,
    kiln::gfx::renderer::Buffer&        materials_buffer,
    kiln::gfx::renderer::Buffer&        instance_buffer,
    kiln::gfx::renderer::Buffer&        original_draw_commands_buffer,
    kiln::gfx::renderer::Buffer&        generated_draw_commands_buffer
) -> void
{
    if (instance_draw_command_indices_buffer.size() != 0)
    {
        stage_instance_draw_command_indices_buffer(
            allocator,
            staging_stream,
            model_loaders,
            instance_draw_command_indices_buffer
        );
    }
    if (instance_sphere_bounding_volumes_buffer.size() != 0)
    {
        stage_instance_sphere_bounding_volumes_buffer(
            allocator,
            staging_stream,
            model_loaders,
            instance_sphere_bounding_volumes_buffer
        );
    }

    if (geometry_buffer.size() != 0)
    {
        stage_geometry_buffer(allocator, staging_stream, model_loaders, geometry_buffer);
    }
    if (materials_buffer.size() != 0)
    {
        stage_materials_buffer(allocator, staging_stream, model_loaders, materials_buffer);
    }
    if (instance_buffer.size() != 0)
    {
        stage_instance_buffer(allocator, staging_stream, model_loaders, instance_buffer);
    }
    if (generated_draw_commands_buffer.size() != 0)
    {
        PRECOND(
            disable_culling
            || generated_draw_commands_buffer.size()
                   == align_up(sizeof(uint32_t), alignof(shaders::DrawCommand))
                          + original_draw_commands_buffer.size()
        );

        if (!disable_culling)
        {
            stage_draw_commands(
                allocator,
                staging_stream,
                model_loaders,
                original_draw_commands_buffer
            );
        }

        if (disable_culling)
        {
            stage_draw_command_count(
                allocator,
                staging_stream,
                model_loaders,
                kiln::gfx::renderer::BufferRegion{
                    generated_draw_commands_buffer,
                    0,
                    sizeof(uint32_t),
                }
            );
            stage_draw_commands(
                allocator,
                staging_stream,
                model_loaders,
                kiln::gfx::renderer::BufferRegion{
                    generated_draw_commands_buffer,
                    align_up(sizeof(uint32_t), alignof(shaders::DrawCommand)),
                }
            );
        }
    }
}

auto load_scene(
    const std::span<const ModelDescription> model_descriptions,
    const kiln::gfx::renderer::Device&      device,
    kiln::gfx::renderer::Allocator&         gpu_allocator,
    kiln::gfx::renderer::StagingStream&     staging_stream,
    const bool                              disable_culling,
    std::pmr::memory_resource&              transient_memory_resource
) -> Scene
{
    uint32_t                    instance_count{};
    kiln::gfx::renderer::Buffer instance_draw_command_indices_buffer;
    kiln::gfx::renderer::Buffer instance_sphere_bounding_volumes_buffer;
    kiln::gfx::renderer::Buffer draw_command_instance_counts_buffer;
    kiln::gfx::renderer::Buffer instance_draw_command_offsets_buffer;

    uint32_t                    max_draw_command_count{};
    kiln::gfx::renderer::Buffer original_draw_commands_buffer;
    kiln::gfx::renderer::Buffer instance_counter_buffer;
    kiln::gfx::renderer::Buffer generated_draw_commands_buffer;
    kiln::gfx::renderer::Buffer draw_command_instance_offsets_buffer;

    kiln::gfx::renderer::Buffer instance_indices_buffer;

    kiln::gfx::renderer::Buffer geometry_buffer;
    kiln::gfx::renderer::Buffer materials_buffer;
    kiln::gfx::renderer::Buffer instance_buffer;

    {
        std::pmr::vector<GltfModelLoader> model_loaders{ &transient_memory_resource };
        model_loaders.reserve(model_descriptions.size());
        for (const auto& [model_asset, scene_index, transform] : model_descriptions)
        {
            model_loaders.emplace_back(model_asset, scene_index, transform);
        }


        instance_count = std::ranges::fold_left(
            model_loaders | std::views::transform(&GltfModelLoader::instance_count),
            0u,
            std::plus{}
        );
        instance_draw_command_indices_buffer
            = create_instance_draw_command_indices_buffer(
                device,
                gpu_allocator,
                model_loaders
            );
        instance_sphere_bounding_volumes_buffer
            = create_instance_sphere_bounding_volumes_buffer(
                device,
                gpu_allocator,
                model_loaders
            );
        draw_command_instance_counts_buffer = create_draw_command_instance_counts_buffer(
            device,
            gpu_allocator,
            model_loaders
        );
        instance_draw_command_offsets_buffer
            = create_instance_draw_command_offsets_buffer(
                device,
                gpu_allocator,
                model_loaders
            );

        max_draw_command_count = std::ranges::fold_left(
            model_loaders | std::views::transform(&GltfModelLoader::draw_command_count),
            0u,
            std::plus{}
        );
        original_draw_commands_buffer = create_original_draw_commands_buffer(
            device,
            gpu_allocator,
            max_draw_command_count
        );
        instance_counter_buffer = create_instance_counter_buffer(device, gpu_allocator);
        generated_draw_commands_buffer = create_generated_draw_commands_buffer(
            device,
            gpu_allocator,
            max_draw_command_count
        );
        draw_command_instance_offsets_buffer
            = create_draw_command_instance_offsets_buffer(
                device,
                gpu_allocator,
                max_draw_command_count
            );

        instance_indices_buffer
            = create_instance_indices_buffer(device, gpu_allocator, instance_count);

        geometry_buffer  = create_geometry_buffer(device, gpu_allocator, model_loaders);
        materials_buffer = create_materials_buffer(device, gpu_allocator, model_loaders);
        instance_buffer  = create_instance_buffer(device, gpu_allocator, model_loaders);


        stage_models(
            gpu_allocator,
            staging_stream,
            model_loaders,
            disable_culling,
            instance_draw_command_indices_buffer,
            instance_sphere_bounding_volumes_buffer,
            geometry_buffer,
            materials_buffer,
            instance_buffer,
            original_draw_commands_buffer,
            generated_draw_commands_buffer
        );

        gpu_allocator.try_flush(geometry_buffer);
        gpu_allocator.try_flush(materials_buffer);
        gpu_allocator.try_flush(instance_buffer);
        gpu_allocator.try_flush(generated_draw_commands_buffer);

        if (!staging_stream.empty())
        {
            staging_stream.flush(gpu_allocator);
        }
    }

    staging_stream.reset(device);

    return Scene{
        device,
        instance_count,
        std::move(instance_draw_command_indices_buffer),
        std::move(instance_sphere_bounding_volumes_buffer),
        std::move(draw_command_instance_counts_buffer),
        std::move(instance_draw_command_offsets_buffer),
        max_draw_command_count,
        std::move(original_draw_commands_buffer),
        std::move(instance_counter_buffer),
        std::move(generated_draw_commands_buffer),
        std::move(draw_command_instance_offsets_buffer),
        std::move(instance_indices_buffer),
        std::move(geometry_buffer),
        std::move(materials_buffer),
        std::move(instance_buffer),
    };
}

}   // namespace demo
