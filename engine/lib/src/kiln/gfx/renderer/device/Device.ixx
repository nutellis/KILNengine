module;

#include <memory_resource>
#include <span>
#include <string>
#include <vector>

export module kiln.gfx.renderer.device.Device;

import vulkan;

import kiln.gfx.vulkan.PhysicalDeviceCapabilities;
import kiln.gfx.vulkan.QueueFamilyIndex;
import kiln.gfx.vulkan.QueueFamilyInfo;
import kiln.reg.BuildDirector;
import kiln.reg.EntryTraits;

namespace kiln::gfx::renderer {

export class Device {
public:
    using allocator_type = std::pmr::polymorphic_allocator<>;


    Device(Device&&, const allocator_type& allocator);

    explicit Device(
        vk::raii::PhysicalDevice&&                  physical_device,
        vk::raii::Device&&                          logical_device,
        vulkan::PhysicalDeviceCapabilities&&        capabilities,
        std::pmr::vector<vulkan::QueueFamilyInfo>&& queue_family_infos
    );
    explicit Device(
        std::allocator_arg_t,
        const allocator_type&                       allocator,
        vk::raii::PhysicalDevice&&                  physical_device,
        vk::raii::Device&&                          logical_device,
        vulkan::PhysicalDeviceCapabilities&&        capabilities,
        std::pmr::vector<vulkan::QueueFamilyInfo>&& queue_family_infos
    );


    [[nodiscard]]
    auto get_allocator() const noexcept -> allocator_type;

    [[nodiscard]]
    auto name() const -> std::string;
    [[nodiscard]]
    auto physical_device() const noexcept -> const vk::raii::PhysicalDevice&;
    [[nodiscard]]
    auto logical_device() const noexcept -> const vk::raii::Device&;
    [[nodiscard]]
    auto capabilities() const noexcept -> const vulkan::PhysicalDeviceCapabilities&;
    [[nodiscard]]
    auto queue_family(vulkan::QueueFamilyIndex family_index) const noexcept
        -> const vulkan::QueueFamilyInfo&;
    [[nodiscard]]
    auto queue_families() const noexcept -> std::span<const vulkan::QueueFamilyInfo>;

private:
    vk::raii::PhysicalDevice                  m_physical_device;
    vk::raii::Device                          m_logical_device;
    vulkan::PhysicalDeviceCapabilities        m_capabilities;
    std::pmr::vector<vulkan::QueueFamilyInfo> m_queue_family_infos;
};

}   // namespace kiln::gfx::renderer

template <>
struct kiln::reg::EntryTraits<kiln::gfx::renderer::Device> {
    static auto describe_build(BuildDirector<gfx::renderer::Device>& build_director)
        -> void;
};
