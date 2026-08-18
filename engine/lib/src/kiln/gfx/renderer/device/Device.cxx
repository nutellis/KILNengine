module;

#include <cassert>
#include <memory_resource>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "kiln/util/contract_macros.hpp"

module kiln.gfx.renderer.device.Device;

import kiln.gfx.renderer.device.DeviceBuilder;
import kiln.gfx.vulkan.QueueFamilyIndex;
import kiln.gfx.vulkan.QueueFamilyInfo;
import kiln.util.contracts;

namespace kiln::gfx::renderer {

Device::Device(Device&& other, const allocator_type& allocator)
    : m_physical_device{ std::move(other.m_physical_device) },
      m_logical_device{ std::move(other.m_logical_device) },
      m_capabilities{ std::move(other.m_capabilities), allocator },
      m_queue_family_infos{ std::move(other.m_queue_family_infos), allocator }
{
}

Device::Device(
    vk::raii::PhysicalDevice&&                  physical_device,
    vk::raii::Device&&                          logical_device,
    vulkan::PhysicalDeviceCapabilities&&        capabilities,
    std::pmr::vector<vulkan::QueueFamilyInfo>&& queue_family_infos
)
    : Device{
          std::allocator_arg,
          std::pmr::get_default_resource(),   //
          std::move(physical_device),
          std::move(logical_device),
          std::move(capabilities),
          std::move(queue_family_infos),
      }
{
}

Device::Device(
    std::allocator_arg_t,
    const allocator_type&                       allocator,
    vk::raii::PhysicalDevice&&                  physical_device,
    vk::raii::Device&&                          logical_device,
    vulkan::PhysicalDeviceCapabilities&&        capabilities,
    std::pmr::vector<vulkan::QueueFamilyInfo>&& queue_family_infos
)
    : m_physical_device{ std::move(physical_device) },
      m_logical_device{ std::move(logical_device) },
      m_capabilities{ std::move(capabilities), allocator },
      m_queue_family_infos{ std::move(queue_family_infos), allocator }
{
}

auto Device::get_allocator() const noexcept -> allocator_type
{
    return m_capabilities.get_allocator();
}

auto Device::name() const -> std::string
{
    return m_physical_device.getProperties2().properties.deviceName;
}

auto Device::physical_device() const noexcept -> const vk::raii::PhysicalDevice&
{
    return m_physical_device;
}

auto Device::logical_device() const noexcept -> const vk::raii::Device&
{
    return m_logical_device;
}

auto Device::capabilities() const noexcept -> const vulkan::PhysicalDeviceCapabilities&
{
    return m_capabilities;
}

auto Device::queue_family(const vulkan::QueueFamilyIndex family_index) const noexcept
    -> const vulkan::QueueFamilyInfo&
{
    PRECOND(family_index.underlying() < m_queue_family_infos.size());

    const vulkan::QueueFamilyInfo& result{
        m_queue_family_infos.at(family_index.underlying())
    };
    assert(result.index() == family_index);

    return result;
}

auto Device::queue_families() const noexcept -> std::span<const vulkan::QueueFamilyInfo>
{
    return m_queue_family_infos;
}

}   // namespace kiln::gfx::renderer

auto kiln::reg::EntryTraits<kiln::gfx::renderer::Device>::describe_build(
    BuildDirector<gfx::renderer::Device>& build_director
) -> void
{
    build_director.use_builder<gfx::renderer::DeviceBuilder>();
}
