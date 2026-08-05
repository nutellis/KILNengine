module;

#include <cstdint>
#include <memory_resource>
#include <span>
#include <vector>

export module kiln.gfx.vulkan.PhysicalDeviceCapabilities;

import vulkan;

import kiln.gfx.vulkan.structure_chain.core_feature_struct_from_vulkan1x_c;
import kiln.gfx.vulkan.structure_chain.feature_struct_c;
import kiln.gfx.vulkan.structure_chain.individual_feature_struct_c;
import kiln.gfx.vulkan.structure_chain.match_physical_device_features;
import kiln.gfx.vulkan.structure_chain.merge_physical_device_features;
import kiln.gfx.vulkan.structure_chain.erase_physical_device_features;
import kiln.gfx.vulkan.structure_chain.StructureChain;
import kiln.gfx.vulkan.structure_chain.vulkan1x_feature_struct_c;
import kiln.util.containers.OptionalRef;
import kiln.util.StringLiteral;

namespace kiln::gfx::vulkan {

export class PhysicalDeviceCapabilities {
public:
    using allocator_type = std::pmr::polymorphic_allocator<>;


    PhysicalDeviceCapabilities(
        const PhysicalDeviceCapabilities&,
        const allocator_type& allocator
    );
    PhysicalDeviceCapabilities(
        PhysicalDeviceCapabilities&&,
        const allocator_type& allocator
    );

    explicit PhysicalDeviceCapabilities() = default;
    explicit PhysicalDeviceCapabilities(const allocator_type& allocator);


    [[nodiscard]]
    auto get_allocator() const noexcept -> allocator_type;

    [[nodiscard]]
    auto version() const noexcept -> uint32_t;
    [[nodiscard]]
    auto extensions() const noexcept -> std::span<const util::StringLiteral>;
    [[nodiscard]]
    auto features_chain() const noexcept
        -> const StructureChain<vk::PhysicalDeviceFeatures2>&;
    template <feature_struct_c FeaturesStruct_T>
    [[nodiscard]]
    auto contains_features(const FeaturesStruct_T& features) const noexcept -> bool;

    [[nodiscard]]
    auto supported_by(const vk::raii::PhysicalDevice& physical_device) const -> bool;

    auto filter_uncontained_features(vk::PhysicalDeviceFeatures& features) const -> void;
    template <vulkan1x_feature_struct_c Vulkan1XFeatureStruct_T>
    auto filter_uncontained_features(Vulkan1XFeatureStruct_T& features) const -> void;
    template <individual_feature_struct_c FeaturesStruct_T>
    auto filter_uncontained_features(FeaturesStruct_T& features) const -> void;


    auto upgrade_version(uint32_t new_version) -> void;

    auto insert_extension(util::StringLiteral extension_name) -> void;

    auto insert_features(const vk::PhysicalDeviceFeatures& features) -> void;
    auto insert_features(const vk::PhysicalDeviceVulkan11Features& features) -> void;
    auto insert_features(const vk::PhysicalDeviceVulkan12Features& features) -> void;
    auto insert_features(const vk::PhysicalDeviceVulkan13Features& features) -> void;
    auto insert_features(const vk::PhysicalDeviceVulkan14Features& features) -> void;
    template <individual_feature_struct_c FeaturesStruct_T>
    auto insert_features(const FeaturesStruct_T& features) -> void;
    auto insert_features(StructureChain<vk::PhysicalDeviceFeatures2> features) -> void;

    auto insert(const PhysicalDeviceCapabilities& other) -> void;


    auto erase_extension(util::StringLiteral extension_name) -> void;

    auto erase_features(const vk::PhysicalDeviceFeatures& features) -> void;
    auto erase_features(const vk::PhysicalDeviceVulkan11Features& features) -> void;
    auto erase_features(const vk::PhysicalDeviceVulkan12Features& features) -> void;
    auto erase_features(const vk::PhysicalDeviceVulkan13Features& features) -> void;
    auto erase_features(const vk::PhysicalDeviceVulkan14Features& features) -> void;
    template <individual_feature_struct_c FeaturesStruct_T>
    auto erase_features(const FeaturesStruct_T& features) -> void;

    auto erase(const PhysicalDeviceCapabilities& other) -> void;

private:
    uint32_t                                    m_version{ vk::ApiVersion10 };
    std::pmr::vector<util::StringLiteral>       m_extension_names{};
    StructureChain<vk::PhysicalDeviceFeatures2> m_features;

    auto upgrade_to_Vulkan11() -> void;
    auto upgrade_to_Vulkan12() -> void;
    auto upgrade_to_Vulkan13() -> void;
    auto upgrade_to_Vulkan14() -> void;
};

}   // namespace kiln::gfx::vulkan

namespace kiln::gfx::vulkan {

template <feature_struct_c FeaturesStruct_T>
auto PhysicalDeviceCapabilities::contains_features(
    const FeaturesStruct_T& features
) const noexcept -> bool
{
    if constexpr (core_feature_struct_from_vulkan11_c<FeaturesStruct_T>)
    {
        if (m_version >= vk::ApiVersion11)
        {
            return m_features.find<vk::PhysicalDeviceVulkan11Features>()
                .transform(
                    [&features](
                        const vk::PhysicalDeviceVulkan11Features& contained
                    ) -> bool
                    {
                        vk::PhysicalDeviceVulkan11Features requested{};
                        merge_physical_device_features(requested, features);
                        return match_physical_device_features(requested, contained);
                    }
                )
                .value_or(false);
        }
    }
    if constexpr (core_feature_struct_from_vulkan12_c<FeaturesStruct_T>)
    {
        if (m_version >= vk::ApiVersion12)
        {
            return m_features.find<vk::PhysicalDeviceVulkan12Features>()
                .transform(
                    [&features](
                        const vk::PhysicalDeviceVulkan12Features& contained
                    ) -> bool
                    {
                        vk::PhysicalDeviceVulkan12Features requested{};
                        merge_physical_device_features(requested, features);
                        return match_physical_device_features(requested, contained);
                    }
                )
                .value_or(false);
        }
    }
    if constexpr (core_feature_struct_from_vulkan13_c<FeaturesStruct_T>)
    {
        if (m_version >= vk::ApiVersion13)
        {
            return m_features.find<vk::PhysicalDeviceVulkan13Features>()
                .transform(
                    [&features](
                        const vk::PhysicalDeviceVulkan13Features& contained
                    ) -> bool
                    {
                        vk::PhysicalDeviceVulkan13Features requested{};
                        merge_physical_device_features(requested, features);
                        return match_physical_device_features(requested, contained);
                    }
                )
                .value_or(false);
        }
    }
    if constexpr (core_feature_struct_from_vulkan14_c<FeaturesStruct_T>)
    {
        if (m_version >= vk::ApiVersion14)
        {
            return m_features.find<vk::PhysicalDeviceVulkan14Features>()
                .transform(
                    [&features](
                        const vk::PhysicalDeviceVulkan14Features& contained
                    ) -> bool
                    {
                        vk::PhysicalDeviceVulkan14Features requested{};
                        merge_physical_device_features(requested, features);
                        return match_physical_device_features(requested, contained);
                    }
                )
                .value_or(false);
        }
    }

    return m_features.find<FeaturesStruct_T>()
        .transform(
            [&features](const FeaturesStruct_T& contained) -> bool
            { return match_physical_device_features(features, contained); }
        )
        .value_or(false);
}

template <vulkan1x_feature_struct_c Vulkan1XFeatureStruct_T>
auto PhysicalDeviceCapabilities::filter_uncontained_features(
    Vulkan1XFeatureStruct_T& features
) const -> void
{
    if (const util::OptionalRef<const Vulkan1XFeatureStruct_T> contained_features{
            m_features.find<Vulkan1XFeatureStruct_T>() };
        contained_features.has_value())
    {
        erase_physical_device_features(features, *contained_features);
    }
}

template <individual_feature_struct_c FeaturesStruct_T>
auto PhysicalDeviceCapabilities::filter_uncontained_features(
    FeaturesStruct_T& features
) const -> void
{
    if constexpr (core_feature_struct_from_vulkan11_c<FeaturesStruct_T>)
    {
        if (m_version >= vk::ApiVersion11)
        {
            if (const util::OptionalRef<const vk::PhysicalDeviceVulkan11Features>
                    contained_features{
                        m_features.find<vk::PhysicalDeviceVulkan11Features>() };
                contained_features.has_value())
            {
                erase_physical_device_features(features, *contained_features);
            }
            return;
        }
    }
    else if constexpr (core_feature_struct_from_vulkan12_c<FeaturesStruct_T>)
    {
        if (m_version >= vk::ApiVersion12)
        {
            if (const util::OptionalRef<const vk::PhysicalDeviceVulkan12Features>
                    contained_features{
                        m_features.find<vk::PhysicalDeviceVulkan12Features>() };
                contained_features.has_value())
            {
                erase_physical_device_features(features, *contained_features);
            }
            return;
        }
    }
    else if constexpr (core_feature_struct_from_vulkan13_c<FeaturesStruct_T>)
    {
        if (m_version >= vk::ApiVersion13)
        {
            if (const util::OptionalRef<const vk::PhysicalDeviceVulkan13Features>
                    contained_features{
                        m_features.find<vk::PhysicalDeviceVulkan13Features>() };
                contained_features.has_value())
            {
                erase_physical_device_features(features, *contained_features);
            }
            return;
        }
    }
    else if constexpr (core_feature_struct_from_vulkan14_c<FeaturesStruct_T>)
    {
        if (m_version >= vk::ApiVersion14)
        {
            if (const util::OptionalRef<const vk::PhysicalDeviceVulkan14Features>
                    contained_features{
                        m_features.find<vk::PhysicalDeviceVulkan14Features>() };
                contained_features.has_value())
            {
                erase_physical_device_features(features, *contained_features);
            }
            return;
        }
    }

    if (const util::OptionalRef<const FeaturesStruct_T> contained_features{
            m_features.find<FeaturesStruct_T>() };
        contained_features.has_value())
    {
        erase_physical_device_features(features, *contained_features);
    }
}

template <individual_feature_struct_c FeaturesStruct_T>
auto PhysicalDeviceCapabilities::insert_features(const FeaturesStruct_T& features) -> void
{
    if constexpr (core_feature_struct_from_vulkan11_c<FeaturesStruct_T>)
    {
        if (m_version >= vk::ApiVersion11)
        {
            vk::PhysicalDeviceVulkan11Features comprehensive_features{};
            merge_physical_device_features(comprehensive_features, features);
            m_features.merge(comprehensive_features);
            return;
        }
    }
    else if constexpr (core_feature_struct_from_vulkan12_c<FeaturesStruct_T>)
    {
        if (m_version >= vk::ApiVersion12)
        {
            vk::PhysicalDeviceVulkan12Features comprehensive_features{};
            merge_physical_device_features(comprehensive_features, features);
            m_features.merge(comprehensive_features);
            return;
        }
    }
    else if constexpr (core_feature_struct_from_vulkan13_c<FeaturesStruct_T>)
    {
        if (m_version >= vk::ApiVersion13)
        {
            vk::PhysicalDeviceVulkan13Features comprehensive_features{};
            merge_physical_device_features(comprehensive_features, features);
            m_features.merge(comprehensive_features);
            return;
        }
    }
    else if constexpr (core_feature_struct_from_vulkan14_c<FeaturesStruct_T>)
    {
        if (m_version >= vk::ApiVersion14)
        {
            vk::PhysicalDeviceVulkan14Features comprehensive_features{};
            merge_physical_device_features(comprehensive_features, features);
            m_features.merge(comprehensive_features);
            return;
        }
    }

    m_features.merge(features);
}

template <individual_feature_struct_c FeaturesStruct_T>
auto PhysicalDeviceCapabilities::erase_features(const FeaturesStruct_T& features) -> void
{
    m_features.erase_features(features);
}

}   // namespace kiln::gfx::vulkan
