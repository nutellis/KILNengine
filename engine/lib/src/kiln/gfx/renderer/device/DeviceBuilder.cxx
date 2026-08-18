module;

#include <algorithm>
#include <memory_resource>
#include <ranges>
#include <span>
#include <utility>

module kiln.gfx.renderer.device.DeviceBuilder;

import vulkan;

import kiln.gfx.renderer.device.Device;
import kiln.gfx.renderer.device.DeviceBuildFailedError;
import kiln.gfx.vulkan.result.check_result;
import kiln.gfx.vulkan.structure_chain.StructureChain;

namespace kiln::gfx::renderer {

[[nodiscard]]
auto make_device_builder(app::MemoryArena& memory_arena) -> DeviceBuilder
{
    return DeviceBuilder{ memory_arena.pool_allocator() };
}

auto describe_build(reg::BuildDirector<DeviceBuilder>& build_director) -> void
{
    build_director.use_function<make_device_builder>();
}

DeviceBuilder::DeviceBuilder(const DeviceBuilder& other, const allocator_type& allocator)
    : m_physical_device_filter{ other.m_physical_device_filter, allocator },
      m_optional_capabilities{ other.m_optional_capabilities, allocator }
{
}

DeviceBuilder::DeviceBuilder(DeviceBuilder&& other, const allocator_type& allocator)
    : m_physical_device_filter{ std::move(other.m_physical_device_filter), allocator },
      m_optional_capabilities{ std::move(other.m_optional_capabilities), allocator }
{
}

DeviceBuilder::DeviceBuilder(const allocator_type& allocator)
    : m_physical_device_filter{ allocator },
      m_optional_capabilities{ allocator }
{
}

DeviceBuilder::DeviceBuilder(vulkan::PhysicalDeviceFilter&& physical_device_selector)
    : DeviceBuilder{
          std::allocator_arg,
          std::pmr::get_default_resource(),
          std::move(physical_device_selector),
      }
{
}

DeviceBuilder::DeviceBuilder(
    std::allocator_arg_t,
    const allocator_type&          allocator,
    vulkan::PhysicalDeviceFilter&& physical_device_selector
)
    : m_physical_device_filter{ std::move(physical_device_selector), allocator }
{
}

auto DeviceBuilder::get_allocator() const -> allocator_type
{
    return m_physical_device_filter.get_allocator();
}

auto device_queue_create_infos_from(
    const std::span<const vulkan::QueueFamilyInfo> family_infos,
    const std::pmr::polymorphic_allocator<>&       allocator
) -> std::pmr::vector<vk::DeviceQueueCreateInfo>
{
    std::pmr::vector<vk::DeviceQueueCreateInfo> result{ allocator };
    result.reserve(
        std::ranges::fold_left(
            std::views::transform(family_infos, &vulkan::QueueFamilyInfo::queue_count),
            0,
            std::plus{}
        )
    );

    for (const vulkan::QueueFamilyInfo& family_info : family_infos)
    {
        for (const auto& create_info : family_info.create_infos())
        {
            result.push_back(static_cast<const vk::DeviceQueueCreateInfo&>(create_info));
        }
    }

    return result;
}

auto DeviceBuilder::build(
    app::MemoryArena&           memory_arena,
    const vulkan::Instance&     instance,
    const wsi::Context&         wsi_context,
    const QueueProviderBuilder& queue_provider_builder
) const -> Device
{
    std::pmr::monotonic_buffer_resource transient_memory_resource{
        memory_arena.make_transient_resource()
    };

    const vulkan::PhysicalDeviceFilter physical_device_filter{
        auto{ m_physical_device_filter }
            .add_custom_requirement(queue_provider_builder.device_requirement())
    };

    std::vector<vk::raii::PhysicalDevice> supported_devices{
        vulkan::check_result(instance.get().enumeratePhysicalDevices())
    };
    std::erase_if(
        supported_devices,
        [&physical_device_filter](const vk::raii::PhysicalDevice& physical_device) -> bool
        {
            return !physical_device_filter.is_adequate(physical_device);   //
        }
    );

    if (supported_devices.empty())
    {
        throw DeviceBuildFailedError{ "No supported device found" };
    }

    vk::raii::PhysicalDevice physical_device{ std::move(supported_devices.front()) };

    vulkan::PhysicalDeviceCapabilities capabilities{
        physical_device_filter.required_capabilities()
    };

    for (
        const std::vector<vk::ExtensionProperties> supported_extension_properties{
            vulkan::check_result(physical_device.enumerateDeviceExtensionProperties()),
        };
        const util::StringLiteral optional_extension :
        m_optional_capabilities.extensions()
    )
    {
        if (std::ranges::any_of(
                supported_extension_properties,
                [optional_extension](const vk::ExtensionProperties& extension_properties)
                    -> bool
                {
                    return optional_extension
                        == util::StringLiteral::unsafe_create(
                               extension_properties.extensionName
                        );
                }
            ))
        {
            capabilities.insert_extension(optional_extension);
        }
    }

    vulkan::StructureChain<vk::PhysicalDeviceFeatures2> supported_optional_features{
        m_optional_capabilities.features_chain(),
        &transient_memory_resource
    };
    vulkan::StructureChain<vk::PhysicalDeviceFeatures2> optional_feature_support{
        m_optional_capabilities.features_chain(),
        &transient_memory_resource
    };
    physical_device.getDispatcher()
        ->vkGetPhysicalDeviceFeatures2(*physical_device, optional_feature_support.root());
    supported_optional_features.erase_unsupported_features(
        optional_feature_support.root()
    );
    capabilities.insert_features(supported_optional_features);

    std::pmr::vector queue_family_infos{
        queue_provider_builder.create_queue_family_infos(
            instance,
            wsi_context,
            physical_device,
            memory_arena.pool_allocator()
        ),
    };
    const std::pmr::vector queue_create_infos{
        device_queue_create_infos_from(queue_family_infos, &transient_memory_resource)
    };

    const vk::DeviceCreateInfo device_create_info{
        .pNext                 = &capabilities.features_chain().root(),
        .queueCreateInfoCount  = static_cast<uint32_t>(queue_create_infos.size()),
        .pQueueCreateInfos     = queue_create_infos.data(),
        .enabledExtensionCount = static_cast<uint32_t>(capabilities.extensions().size()),
        .ppEnabledExtensionNames = capabilities.extensions().empty()
                                     ? nullptr
                                     : capabilities.extensions().front().address(),
    };

    vk::raii::Device device{
        vulkan::check_result(physical_device.createDevice(device_create_info))
    };

    return Device{
        std::allocator_arg,
        memory_arena.pool_allocator(),   //
        std::move(physical_device),
        std::move(device),
        std::move(capabilities),
        std::move(queue_family_infos),
    };
}

}   // namespace kiln::gfx::renderer
