module;

#include <format>
#include <ranges>
#include <sstream>

#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <vulkan/vk_platform.h>   // used for VKAPI_ATTR and VKAPI_CALL

module kiln.gfx.vulkan.DebugMessenger;

import kiln.reg.BuildableEntryBuilder;
import kiln.reg.BuildDirector;
import kiln.config.engine_name;
import kiln.gfx.vulkan.Instance;
import kiln.gfx.vulkan.InstanceBuilder;
import kiln.gfx.vulkan.result.check_result;

namespace kiln::gfx::vulkan {

const auto logger{
    [] -> auto
    {
        auto result{
            spdlog::stdout_color_mt(std::format("{}:Vulkan", config::engine_name()))
        };
        result->set_level(spdlog::level::level_enum::debug);
        result->set_pattern("%^[%n](%r) %l:\n- %v%$");
        return result;
    }(),
};

[[nodiscard]]
// ReSharper disable once CppNotAllPathsReturnValue
constexpr auto log_level_of(const vk::DebugUtilsMessageSeverityFlagBitsEXT severity)
    -> spdlog::level::level_enum
{
    switch (severity)
    {
        case vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose:
            return spdlog::level::debug;
        case vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo: return spdlog::level::info;
        case vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning:
            return spdlog::level::warn;
        case vk::DebugUtilsMessageSeverityFlagBitsEXT::eError: return spdlog::level::err;
    }
}

VKAPI_ATTR auto VKAPI_CALL default_debug_messenger_callback(
    const vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
    const vk::DebugUtilsMessageTypeFlagsEXT,
    const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* const
) -> vk::Bool32
{
    std::ostringstream message;

    message << std::format("{}\n", pCallbackData->pMessage);

    if (pCallbackData->queueLabelCount > 0)
    {
        message << "- Queue labels:\n";
        for (
            const char* label_name :
            std::span{ pCallbackData->pQueueLabels, pCallbackData->queueLabelCount }
                | std::views::transform(&vk::DebugUtilsLabelEXT::pLabelName)
        )
        {
            message << std::format("\t- \"{}\"\n", label_name);
        }
    }

    if (pCallbackData->cmdBufLabelCount > 0)
    {
        message << "- Command buffer labels:\n";
        for (
            const char* label_name :
            std::span{ pCallbackData->pCmdBufLabels, pCallbackData->cmdBufLabelCount }
                | std::views::transform(&vk::DebugUtilsLabelEXT::pLabelName)
        )
        {
            message << std::format("\t- \"{}\"\n", label_name);
        }
    }

    if (pCallbackData->objectCount > 0)
    {
        message << "- Objects:\n";
        for (
            const vk::DebugUtilsObjectNameInfoEXT& object_info :
            std::span{ pCallbackData->pObjects, pCallbackData->objectCount }
        )
        {
            const char* const object_name{
                object_info.pObjectName != nullptr ? object_info.pObjectName : "",
            };
            message << std::format(
                "\t- {} ({}) {}\n",
                vk::to_string(object_info.objectType),
                object_info.objectHandle,
                object_name
            );
        }
    }

    logger->log(log_level_of(severity), message.str());

    return vk::False;
}

class DebugMessengerBuilder;

auto describe_build(reg::BuildDirector<DebugMessengerBuilder>& build_director) -> void;

class DebugMessengerBuilder
    : public reg::
          BuildableEntryBuilder<DebugMessenger, DebugMessengerBuilder, describe_build> {
public:
    [[nodiscard]]
    // ReSharper disable once CppDeclaratorNeverUsed
    static auto create(InstanceBuilder& instance_builder) -> DebugMessengerBuilder
    {
        instance_builder.enable_layer("VK_LAYER_KHRONOS_validation");
        instance_builder.enable_extension(vk::EXTDebugUtilsExtensionName);

        return DebugMessengerBuilder{};
    }

    [[nodiscard]]
    // ReSharper disable once CppDeclaratorNeverUsed
    static auto build(const Instance& instance) -> DebugMessenger
    {
        constexpr static vk::DebugUtilsMessengerCreateInfoEXT debug_messenger_create_info{
            .messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
                             | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
            .messageType     = vk::DebugUtilsMessageTypeFlagBitsEXT::eDeviceAddressBinding
                             | vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
                             | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance
                             | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation,
            .pfnUserCallback = default_debug_messenger_callback,
        };

        return DebugMessenger{
            check_result(
                instance.get().createDebugUtilsMessengerEXT(debug_messenger_create_info)
            ),
        };
    }
};

auto describe_build(reg::BuildDirector<DebugMessengerBuilder>& build_director) -> void
{
    build_director.use_function<DebugMessengerBuilder::create>();
}

DebugMessenger::DebugMessenger(vk::raii::DebugUtilsMessengerEXT&& debug_messenger)
    : m_debug_messenger{ std::move(debug_messenger) }
{
}

}   // namespace kiln::gfx::vulkan

auto kiln::reg::EntryTraits<kiln::gfx::vulkan::DebugMessenger>::describe_build(
    BuildDirector<gfx::vulkan::DebugMessenger>& build_director
) -> void
{
    build_director.use_builder<gfx::vulkan::DebugMessengerBuilder>();
}
