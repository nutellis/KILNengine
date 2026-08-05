module;

#include <cstddef>
#include <memory>
#include <optional>
#include <span>

#include <vk_mem_alloc.h>

#include "kiln/util/memory/lifetimebound.hpp"

export module kiln.gfx.renderer.memory.Allocator;

import vulkan;

import kiln.gfx.renderer.device.Device;
import kiln.gfx.renderer.memory.Allocation;
import kiln.gfx.renderer.memory.Buffer;
import kiln.gfx.renderer.memory.BufferRegion;
import kiln.gfx.renderer.memory.Image;
import kiln.gfx.renderer.memory.LazyCopy;
import kiln.gfx.vulkan.Instance;
import kiln.reg.BuildDirector;
import kiln.reg.EntryTraits;

namespace kiln::gfx::renderer {

export class Allocator {
public:
    // clang-format off
    explicit Allocator(
        const vulkan::Instance&          instance,
        kiln_lifetimebound const Device& device
    );
    // clang-format on


    [[nodiscard]]
    auto get() -> VmaAllocator;


    [[nodiscard]]
    auto create_buffer(
        const vk::BufferCreateInfo&    buffer_create_info,
        const VmaAllocationCreateInfo& allocation_create_info,
        std::optional<uint32_t>        min_alignment = std::nullopt
    ) -> Buffer;
    [[nodiscard]]
    auto create_image(
        const vk::ImageCreateInfo&     image_create_info,
        const VmaAllocationCreateInfo& allocation_create_info
    ) -> Image;

    auto host_copy(
        std::span<const std::byte> source,
        Allocation&                destination,
        vk::DeviceSize             destination_offset,
        vk::DeviceSize             destination_size
    ) -> void;
    auto host_copy(std::span<const std::byte> source, const BufferRegion& destination)
        -> void;
    auto host_copy(const LazyCopy& lazy_copy, const BufferRegion& destination) -> void;

    [[nodiscard]]
    auto map(Allocation& allocation) -> std::span<std::byte>;
    auto map(const BufferRegion& buffer_region) -> std::span<std::byte>;
    auto unmap(Allocation& allocation) -> void;
    auto unmap(const BufferRegion& buffer_region) -> void;

    auto invalidate(
        Allocation&    allocation,
        vk::DeviceSize destination_offset,
        vk::DeviceSize destination_size
    ) -> void;
    auto invalidate(const BufferRegion& buffer) -> void;
    auto try_invalidate(
        Allocation&    allocation,
        vk::DeviceSize destination_offset,
        vk::DeviceSize destination_size
    ) -> bool;
    auto try_invalidate(const BufferRegion& buffer) -> bool;
    auto flush(
        Allocation&    allocation,
        vk::DeviceSize destination_offset,
        vk::DeviceSize destination_size
    ) -> void;
    auto flush(const BufferRegion& buffer) -> void;
    auto try_flush(
        Allocation&    allocation,
        vk::DeviceSize destination_offset,
        vk::DeviceSize destination_size
    ) -> bool;
    auto try_flush(const BufferRegion& buffer) -> bool;

private:
    std::reference_wrapper<const Device>                            m_device;
    std::unique_ptr<VmaAllocator_T, decltype(&vmaDestroyAllocator)> m_handle;
};

}   // namespace kiln::gfx::renderer

template <>
struct kiln::reg::EntryTraits<kiln::gfx::renderer::Allocator> {
    static auto describe_build(BuildDirector<gfx::renderer::Allocator>& build_director)
        -> void;
};
