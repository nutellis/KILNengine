module;

#include <algorithm>
#include <memory>
#include <span>

#include <vk_mem_alloc.h>

#include "kiln/util/contract_macros.hpp"

module kiln.gfx.renderer.memory.Allocator;

import vulkan;

import kiln.gfx.renderer.device.Device;
import kiln.gfx.renderer.memory.Allocation;
import kiln.gfx.renderer.memory.AllocatorBuilder;
import kiln.gfx.renderer.memory.MemoryTypeID;
import kiln.gfx.vulkan.Instance;
import kiln.gfx.vulkan.PhysicalDeviceCapabilities;
import kiln.gfx.vulkan.result.check_result;
import kiln.util.contracts;
import kiln.util.StringLiteral;

namespace kiln::gfx::renderer {

[[nodiscard]]
auto collect_vulkan_functions(
    const vk::raii::Instance& instance,
    const vk::raii::Device&   device
) -> VmaVulkanFunctions
{
    return VmaVulkanFunctions{
        .vkGetInstanceProcAddr = instance.getDispatcher()->vkGetInstanceProcAddr,
        .vkGetDeviceProcAddr   = device.getDispatcher()->vkGetDeviceProcAddr,
        .vkGetPhysicalDeviceProperties
        = instance.getDispatcher()->vkGetPhysicalDeviceProperties,
        .vkGetPhysicalDeviceMemoryProperties
        = instance.getDispatcher()->vkGetPhysicalDeviceMemoryProperties,
        .vkAllocateMemory          = device.getDispatcher()->vkAllocateMemory,
        .vkFreeMemory              = device.getDispatcher()->vkFreeMemory,
        .vkMapMemory               = device.getDispatcher()->vkMapMemory,
        .vkUnmapMemory             = device.getDispatcher()->vkUnmapMemory,
        .vkFlushMappedMemoryRanges = device.getDispatcher()->vkFlushMappedMemoryRanges,
        .vkInvalidateMappedMemoryRanges
        = device.getDispatcher()->vkInvalidateMappedMemoryRanges,
        .vkBindBufferMemory = device.getDispatcher()->vkBindBufferMemory,
        .vkBindImageMemory  = device.getDispatcher()->vkBindImageMemory,
        .vkGetBufferMemoryRequirements
        = device.getDispatcher()->vkGetBufferMemoryRequirements,
        .vkGetImageMemoryRequirements
        = device.getDispatcher()->vkGetImageMemoryRequirements,
        .vkCreateBuffer  = device.getDispatcher()->vkCreateBuffer,
        .vkDestroyBuffer = device.getDispatcher()->vkDestroyBuffer,
        .vkCreateImage   = device.getDispatcher()->vkCreateImage,
        .vkDestroyImage  = device.getDispatcher()->vkDestroyImage,
        .vkCmdCopyBuffer = device.getDispatcher()->vkCmdCopyBuffer,
        .vkGetBufferMemoryRequirements2KHR
        = device.getDispatcher()->vkGetBufferMemoryRequirements2,
        .vkGetImageMemoryRequirements2KHR
        = device.getDispatcher()->vkGetImageMemoryRequirements2,
        .vkBindBufferMemory2KHR = device.getDispatcher()->vkBindBufferMemory2,
        .vkBindImageMemory2KHR  = device.getDispatcher()->vkBindImageMemory2,
        .vkGetPhysicalDeviceMemoryProperties2KHR
        = instance.getDispatcher()->vkGetPhysicalDeviceMemoryProperties2,
        .vkGetDeviceBufferMemoryRequirements
        = device.getDispatcher()->vkGetDeviceBufferMemoryRequirements,
        .vkGetDeviceImageMemoryRequirements
        = device.getDispatcher()->vkGetDeviceImageMemoryRequirements,
    };
}

[[nodiscard]]
auto vma_allocator_create_flags(
    const uint32_t                            vma_vulkan_api_version,
    const vulkan::PhysicalDeviceCapabilities& device_capabilities
) noexcept -> VmaAllocatorCreateFlags
{
    VmaAllocatorCreateFlags flags{};

    flags |= VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT;

    if (vma_vulkan_api_version == vk::ApiVersion10
        && (std::ranges::contains(
            device_capabilities.extensions(),
            util::StringLiteral{ vk::KHRGetMemoryRequirements2ExtensionName }
        ))
        && std::ranges::contains(
            device_capabilities.extensions(),
            util::StringLiteral{ vk::KHRDedicatedAllocationExtensionName }
        ))
    {
        flags |= VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;
    }

    if (vma_vulkan_api_version == vk::ApiVersion10
        && std::ranges::contains(
            device_capabilities.extensions(),
            util::StringLiteral{ vk::KHRBindMemory2ExtensionName }
        ))
    {
        flags |= VMA_ALLOCATOR_CREATE_KHR_BIND_MEMORY2_BIT;
    }

    if (std::ranges::contains(
            device_capabilities.extensions(),
            util::StringLiteral{ vk::EXTMemoryBudgetExtensionName }
        ))
    {
        flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
    }

    constexpr static vk::PhysicalDeviceCoherentMemoryFeaturesAMD
        coherent_memory_features_amd{
            .deviceCoherentMemory = vk::True,
        };
    if (std::ranges::contains(
            device_capabilities.extensions(),
            util::StringLiteral{ vk::AMDDeviceCoherentMemoryExtensionName }
        )
        && device_capabilities.contains_features(coherent_memory_features_amd))
    {
        flags |= VMA_ALLOCATOR_CREATE_AMD_DEVICE_COHERENT_MEMORY_BIT;
    }

    constexpr static vk::PhysicalDeviceBufferDeviceAddressFeatures
        buffer_device_address_features{
            .bufferDeviceAddress = vk::True,
        };
    if ((vma_vulkan_api_version < vk::ApiVersion12
         && std::ranges::contains(
             device_capabilities.extensions(),
             util::StringLiteral{ vk::KHRBufferDeviceAddressExtensionName }
         ))
        || device_capabilities.contains_features(buffer_device_address_features))
    {
        flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    }

    constexpr static vk::PhysicalDeviceMemoryPriorityFeaturesEXT memory_priority_features{
        .memoryPriority = vk::True,
    };
    if (std::ranges::contains(
            device_capabilities.extensions(),
            util::StringLiteral{ vk::EXTMemoryPriorityExtensionName }
        )
        && device_capabilities.contains_features(memory_priority_features))
    {
        flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT;
    }

    constexpr static vk::PhysicalDeviceMaintenance4Features maintenance4_features{
        .maintenance4 = vk::True,
    };
    if (device_capabilities.contains_features(maintenance4_features))
    {
        flags |= VMA_ALLOCATOR_CREATE_KHR_MAINTENANCE4_BIT;
    }

    constexpr static vk::PhysicalDeviceMaintenance5Features maintenance5_features{
        .maintenance5 = vk::True,
    };
    if (device_capabilities.contains_features(maintenance5_features))
    {
        flags |= VMA_ALLOCATOR_CREATE_KHR_MAINTENANCE5_BIT;
    }

    if (std::ranges::contains(
            device_capabilities.extensions(),
            util::StringLiteral{ vk::KHRExternalMemoryWin32ExtensionName }
        ))
    {
        flags |= VMA_ALLOCATOR_CREATE_KHR_EXTERNAL_MEMORY_WIN32_BIT;
    }

    return flags;
}

[[nodiscard]]
auto create_allocator(const vulkan::Instance& instance, const Device& device)
    -> std::unique_ptr<VmaAllocator_T, decltype(&vmaDestroyAllocator)>
{
    const VmaVulkanFunctions vulkan_functions{
        collect_vulkan_functions(instance.get(), device.logical_device())
    };
    const uint32_t vma_vulkan_api_version{
        std::min(instance.api_version(), device.capabilities().version())
    };
    const VmaAllocatorCreateInfo create_info{
        .flags
        = vma_allocator_create_flags(vma_vulkan_api_version, device.capabilities()),
        .physicalDevice   = *device.physical_device(),
        .device           = *device.logical_device(),
        .pVulkanFunctions = &vulkan_functions,
        .instance         = *instance.get(),
        .vulkanApiVersion = vma_vulkan_api_version,
    };

    VmaAllocator allocator{};
    vulkan::check_result(::vmaCreateAllocator(&create_info, &allocator));

    return std::unique_ptr<VmaAllocator_T, decltype(&::vmaDestroyAllocator)>{
        allocator,
        vmaDestroyAllocator
    };
}

Allocator::Allocator(const vulkan::Instance& instance, const Device& device)
    : m_device{ device },
      m_handle{ create_allocator(instance, device) }
{
}

auto Allocator::get() -> VmaAllocator
{
    return m_handle.get();
}

auto Allocator::create_buffer(
    const vk::BufferCreateInfo&    buffer_create_info,
    const VmaAllocationCreateInfo& allocation_create_info,
    const std::optional<uint32_t>  min_alignment
) -> Buffer
{
    if (buffer_create_info.size == 0)
    {
        return Buffer{};
    }

    VkBuffer          buffer;
    VmaAllocation     allocation{};
    VmaAllocationInfo allocation_info{};
    const VkResult    result
        = min_alignment.has_value()
            ? vmaCreateBufferWithAlignment(
                  m_handle.get(),
                  reinterpret_cast<const std::remove_cvref_t<
                      decltype(buffer_create_info)>::NativeType*>(&buffer_create_info),
                  &allocation_create_info,
                  *min_alignment,
                  &buffer,
                  &allocation,
                  &allocation_info
              )
            : vmaCreateBuffer(
                  m_handle.get(),
                  reinterpret_cast<const std::remove_cvref_t<
                      decltype(buffer_create_info)>::NativeType*>(&buffer_create_info),
                  &allocation_create_info,
                  &buffer,
                  &allocation,
                  &allocation_info
              );
    vulkan::check_result(result);

    return Buffer{
        vk::raii::Buffer{ m_device.get().logical_device(), buffer },
        buffer_create_info.size,
        Allocation{
                         m_handle.get(),
                         allocation, MemoryTypeID{ allocation_info.memoryType },
                         allocation_info.size,
                         },
    };
}

auto Allocator::create_image(
    const vk::ImageCreateInfo&     image_create_info,
    const VmaAllocationCreateInfo& allocation_create_info
) -> Image
{
    VkImage           image;
    VmaAllocation     allocation{};
    VmaAllocationInfo allocation_info{};
    const VkResult    result = vmaCreateImage(
        m_handle.get(),
        reinterpret_cast<
            const std::remove_cvref_t<decltype(image_create_info)>::NativeType*>(
            &image_create_info
        ),
        &allocation_create_info,
        &image,
        &allocation,
        &allocation_info
    );
    vulkan::check_result(result);

    return Image{
        vk::raii::Image{ m_device.get().logical_device(), image },
        image_create_info.format,
        Allocation{
                        m_handle.get(),
                        allocation, MemoryTypeID{ allocation_info.memoryType },
                        allocation_info.size,
                        },
    };
}

auto Allocator::host_copy(
    const std::span<const std::byte> source,
    Allocation&                      destination,
    const vk::DeviceSize             destination_offset,
    const vk::DeviceSize             destination_size
) -> void
{
    PRECOND(source.size_bytes() == destination_size);
    PRECOND(destination.memory_properties() & vk::MemoryPropertyFlagBits::eHostVisible);
    PRECOND(destination.size() - destination_offset >= destination_size);

    vmaCopyMemoryToAllocation(
        m_handle.get(),
        source.data(),
        destination.get(),
        destination_offset,
        destination_size
    );
}

auto Allocator::host_copy(
    const std::span<const std::byte> source,
    const BufferRegion&              destination
) -> void
{
    PRECOND(source.size_bytes() == destination.size());
    host_copy(source, destination.allocation(), destination.offset(), destination.size());
}

auto Allocator::host_copy(const LazyCopy& lazy_copy, const BufferRegion& destination)
    -> void
{
    const vk::MemoryPropertyFlags memory_properties{
        destination.allocation().memory_properties()
    };
    PRECOND(memory_properties & vk::MemoryPropertyFlagBits::eHostVisible);

    if (destination.size() == 0)
    {
        return;
    }

    lazy_copy(map(destination));
    unmap(destination);
    if (!(memory_properties & vk::MemoryPropertyFlagBits::eHostCoherent))
    {
        flush(destination);
    }
}

auto Allocator::map(Allocation& allocation) -> std::span<std::byte>
{
    PRECOND(allocation.memory_properties() & vk::MemoryPropertyFlagBits::eHostVisible);

    void* mapped_data{};
    vulkan::check_result(vmaMapMemory(m_handle.get(), allocation.get(), &mapped_data));

    return std::span{ static_cast<std::byte*>(mapped_data), allocation.size() };
}

auto Allocator::map(const BufferRegion& buffer_region) -> std::span<std::byte>
{
    return map(buffer_region.allocation())
        .subspan(buffer_region.offset(), buffer_region.size());
}

auto Allocator::unmap(Allocation& allocation) -> void
{
    vmaUnmapMemory(m_handle.get(), allocation.get());
}

auto Allocator::unmap(const BufferRegion& buffer_region) -> void
{
    unmap(buffer_region.allocation());
}

auto Allocator::invalidate(
    Allocation&          allocation,
    const vk::DeviceSize destination_offset,
    const vk::DeviceSize destination_size
) -> void
{
    if (allocation.get() != VmaAllocation{})
    {
        const vk::MemoryPropertyFlags memory_properties{ allocation.memory_properties() };

        PRECOND(memory_properties & vk::MemoryPropertyFlagBits::eHostVisible);

        if (!(memory_properties & vk::MemoryPropertyFlagBits::eHostCoherent))
        {
            vulkan::check_result(
                vmaInvalidateAllocation(
                    m_handle.get(),
                    allocation.get(),
                    destination_offset,
                    destination_size
                )   //
            );
        }
    }
}

auto Allocator::invalidate(const BufferRegion& buffer) -> void
{
    invalidate(buffer.allocation(), buffer.offset(), buffer.size());
}

auto Allocator::try_invalidate(
    Allocation&          allocation,
    const vk::DeviceSize destination_offset,
    const vk::DeviceSize destination_size
) -> bool
{
    if (allocation.get() != VmaAllocation{})
    {
        if (const vk::MemoryPropertyFlags memory_properties{
                allocation.memory_properties() };
            memory_properties & vk::MemoryPropertyFlagBits::eHostVisible
            && !(memory_properties & vk::MemoryPropertyFlagBits::eHostCoherent))
        {
            vulkan::check_result(
                vmaInvalidateAllocation(
                    m_handle.get(),
                    allocation.get(),
                    destination_offset,
                    destination_size
                )   //
            );
            return true;
        }
    }
    return false;
}

auto Allocator::try_invalidate(const BufferRegion& buffer) -> bool
{
    return try_invalidate(buffer.allocation(), buffer.offset(), buffer.size());
}

auto Allocator::flush(
    Allocation&          allocation,
    const vk::DeviceSize destination_offset,
    const vk::DeviceSize destination_size
) -> void
{
    if (allocation.get() != VmaAllocation{})
    {
        const vk::MemoryPropertyFlags memory_properties{ allocation.memory_properties() };

        PRECOND(memory_properties & vk::MemoryPropertyFlagBits::eHostVisible);

        if (!(memory_properties & vk::MemoryPropertyFlagBits::eHostCoherent))
        {
            vulkan::check_result(
                vmaFlushAllocation(
                    m_handle.get(),
                    allocation.get(),
                    destination_offset,
                    destination_size
                )   //
            );
        }
    }
}

auto Allocator::flush(const BufferRegion& buffer) -> void
{
    flush(buffer.allocation(), buffer.offset(), buffer.size());
}

auto Allocator::try_flush(
    Allocation&          allocation,
    const vk::DeviceSize destination_offset,
    const vk::DeviceSize destination_size
) -> bool
{
    if (allocation.get() != VmaAllocation{})
    {
        if (const vk::MemoryPropertyFlags memory_properties{
                allocation.memory_properties() };
            memory_properties & vk::MemoryPropertyFlagBits::eHostVisible
            && !(memory_properties & vk::MemoryPropertyFlagBits::eHostCoherent))
        {
            vulkan::check_result(
                vmaFlushAllocation(
                    m_handle.get(),
                    allocation.get(),
                    destination_offset,
                    destination_size
                )   //
            );
            return true;
        }
    }
    return false;
}

auto Allocator::try_flush(const BufferRegion& buffer) -> bool
{
    return try_flush(buffer.allocation(), buffer.offset(), buffer.size());
}

}   // namespace kiln::gfx::renderer

auto kiln::reg::EntryTraits<kiln::gfx::renderer::Allocator>::describe_build(
    BuildDirector<gfx::renderer::Allocator>& build_director
) -> void
{
    build_director.use_builder<gfx::renderer::AllocatorBuilder>();
}
