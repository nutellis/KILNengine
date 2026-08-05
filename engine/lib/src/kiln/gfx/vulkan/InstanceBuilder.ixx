module;

#include <cstdint>
#include <memory_resource>
#include <optional>
#include <vector>

#include "kiln/util/memory/lifetimebound.hpp"

export module kiln.gfx.vulkan.InstanceBuilder;

import vulkan;

import kiln.gfx.vulkan.Instance;
import kiln.reg.BuildableEntryBuilder;
import kiln.reg.BuildDirector;
import kiln.util.StringLiteral;

namespace kiln::gfx::vulkan {

struct InstanceBuilderPrecondition {
    [[nodiscard]]
    consteval static auto minimum_version() noexcept -> uint32_t;

    [[nodiscard]]
    static auto check_version_support(const vk::raii::Context& context) -> bool;


    explicit InstanceBuilderPrecondition(const vk::raii::Context& context);
};

export class InstanceBuilder;

auto describe_build(reg::BuildDirector<InstanceBuilder>& build_director) -> void;

struct CreateInfo {
    std::optional<util::StringLiteral> engine_name;
    std::optional<uint32_t>            engine_version;
    std::optional<util::StringLiteral> application_name;
    std::optional<uint32_t>            application_version;
};

export class InstanceBuilder
    : InstanceBuilderPrecondition,
      public reg::BuildableEntryBuilder<Instance, InstanceBuilder, describe_build>   //
{
public:
    using allocator_type = std::pmr::polymorphic_allocator<>;
    using CreateInfo     = CreateInfo;

    [[nodiscard]]
    consteval static auto minimum_version() noexcept -> uint32_t;

    [[nodiscard]]
    static auto check_version_support(const vk::raii::Context& context) -> bool;


    InstanceBuilder(const InstanceBuilder&, const allocator_type&);
    InstanceBuilder(InstanceBuilder&&, const allocator_type&);

    // clang-format off
    explicit InstanceBuilder(
        const CreateInfo&        create_info,
        kiln_lifetimebound const vk::raii::Context& context
    );
    // clang-format on
    // clang-format off
    explicit InstanceBuilder(
        std::allocator_arg_t,
        const allocator_type&    allocator,
        const CreateInfo&        create_info,
        kiln_lifetimebound const vk::raii::Context& context
    );
    // clang-format on


    [[nodiscard]]
    auto get_allocator() const noexcept -> allocator_type;

    auto target_api_version(uint32_t api_version) -> void;
    auto require_minimum_version(uint32_t version) -> void;
    auto enable_layer(util::StringLiteral layer_name) -> void;
    auto enable_extension(util::StringLiteral extension_name) -> void;
    auto enable_extension_if_available(util::StringLiteral extension_name) -> bool;

    [[nodiscard]]
    auto build() const -> Instance;

private:
    std::reference_wrapper<const vk::raii::Context> m_context;
    std::optional<util::StringLiteral>              m_application_name;
    std::optional<uint32_t>                         m_application_version;
    std::optional<util::StringLiteral>              m_engine_name;
    std::optional<uint32_t>                         m_engine_version;
    uint32_t                                        m_api_version{ minimum_version() };
    uint32_t                              m_minimum_version{ minimum_version() };
    std::pmr::vector<util::StringLiteral> m_layer_names;
    std::pmr::vector<util::StringLiteral> m_extension_names;
};

}   // namespace kiln::gfx::vulkan

namespace kiln::gfx::vulkan {

consteval auto InstanceBuilderPrecondition::minimum_version() noexcept -> uint32_t
{
    return vk::ApiVersion11;
}

consteval auto InstanceBuilder::minimum_version() noexcept -> uint32_t
{
    return InstanceBuilderPrecondition::minimum_version();
}

}   // namespace kiln::gfx::vulkan
