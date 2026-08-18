module;

#include <algorithm>
#include <flat_map>
#include <memory_resource>
#include <ranges>
#include <type_traits>
#include <utility>
#include <vector>

#include "kiln/util/contract_macros.hpp"

export module kiln.gfx.vulkan.structure_chain.StructureChain;

import vulkan;

import kiln.gfx.vulkan.structure_chain.erase_physical_device_features;
import kiln.gfx.vulkan.structure_chain.ErasedStructureChainNode;
import kiln.gfx.vulkan.structure_chain.extends_struct_c;
import kiln.gfx.vulkan.structure_chain.filter_physical_device_features;
import kiln.gfx.vulkan.structure_chain.match_physical_device_features;
import kiln.gfx.vulkan.structure_chain.merge_physical_device_features;
import kiln.gfx.vulkan.structure_chain.StructureChainNode;
import kiln.gfx.vulkan.structure_chain.vulkan1x_feature_struct_c;
import kiln.util.any_cast;
import kiln.util.concepts.naked;
import kiln.util.containers.OptionalRef;
import kiln.util.contracts;
import kiln.util.type_traits.const_like;
import kiln.util.type_traits.forward_like;

namespace kiln::gfx::vulkan {

export template <util::naked_c RootStruct_T>
class StructureChain {
public:
    using allocator_type = std::pmr::polymorphic_allocator<>;
    using NextPtr        = decltype(RootStruct_T::pNext);


    StructureChain(const StructureChain&);
    StructureChain(const StructureChain&, const allocator_type& allocator);
    StructureChain(StructureChain&&) noexcept;
    StructureChain(StructureChain&&, const allocator_type& allocator);

    explicit StructureChain() = default;
    explicit StructureChain(const allocator_type& allocator);


    auto operator=(const StructureChain&) -> StructureChain&;
    auto operator=(StructureChain&&) noexcept -> StructureChain&;


    [[nodiscard]]
    auto get_allocator() const noexcept -> allocator_type;

    template <typename Self_T>
    [[nodiscard]]
    constexpr auto root(this Self_T&&) -> util::forward_like_t<RootStruct_T, Self_T>;

    template <extends_struct_c<RootStruct_T> Struct_T>
    [[nodiscard]]
    constexpr auto contains() const -> bool;
    template <extends_struct_c<RootStruct_T> Struct_T, typename Self_T>
    [[nodiscard]]
    constexpr auto find(
        this Self_T&
    ) -> util::OptionalRef<util::const_like_t<Struct_T, std::remove_reference_t<Self_T>>>;
    template <extends_struct_c<RootStruct_T> Struct_T, typename Self_T>
    [[nodiscard]]
    constexpr auto at(this Self_T&&) -> util::forward_like_t<Struct_T, Self_T>;

    [[nodiscard]]
    constexpr auto matches(
        const vk::PhysicalDeviceFeatures2& physical_device_features
    ) const -> bool
        requires std::same_as<RootStruct_T, vk::PhysicalDeviceFeatures2>;

    template <extends_struct_c<vk::PhysicalDeviceFeatures2> Features_T>
    constexpr auto merge(const Features_T& features) -> void
        requires std::same_as<RootStruct_T, vk::PhysicalDeviceFeatures2>;
    constexpr auto merge(const StructureChain<vk::PhysicalDeviceFeatures2>& other) -> void
        requires std::same_as<RootStruct_T, vk::PhysicalDeviceFeatures2>;

    template <vulkan1x_feature_struct_c Vulkan1XFeatures_T>
    constexpr auto erase_and_merge_features_to_vulkan1x_feature_struct(
        Vulkan1XFeatures_T& vulkan1X_features
    ) -> void
        requires std::same_as<RootStruct_T, vk::PhysicalDeviceFeatures2>;

    constexpr auto erase_unsupported_features(
        const vk::PhysicalDeviceFeatures2& supported_features
    ) -> void
        requires std::same_as<RootStruct_T, vk::PhysicalDeviceFeatures2>;

    constexpr auto erase_features(const vk::PhysicalDeviceFeatures& features) -> void
        requires std::same_as<RootStruct_T, vk::PhysicalDeviceFeatures2>;
    template <extends_struct_c<vk::PhysicalDeviceFeatures2> Features_T>
    constexpr auto erase_features(const Features_T& features) -> void
        requires std::same_as<RootStruct_T, vk::PhysicalDeviceFeatures2>;
    constexpr auto erase_features(
        const vk::PhysicalDeviceFeatures2& physical_device_features
    ) -> void
        requires std::same_as<RootStruct_T, vk::PhysicalDeviceFeatures2>;

private:
    RootStruct_T m_root_struct{};
    std::flat_map<
        vk::StructureType,
        ErasedStructureChainNode<RootStruct_T>,
        std::less<>,
        std::pmr::vector<vk::StructureType>,
        std::pmr::vector<ErasedStructureChainNode<RootStruct_T>>>
        m_chain;

    template <typename Self_T, extends_struct_c<RootStruct_T> Struct_T>
    constexpr auto operator[](this Self_T&&, std::in_place_type_t<Struct_T>)
        -> util::forward_like_t<Struct_T, Self_T>;

    constexpr auto connect() -> void;
};

}   // namespace kiln::gfx::vulkan

namespace kiln::gfx::vulkan {

template <util::naked_c RootStruct_T>
StructureChain<RootStruct_T>::StructureChain(const StructureChain& other)
    : m_root_struct{ other.m_root_struct },
      m_chain{ other.m_chain }
{
    connect();
}

template <util::naked_c RootStruct_T>
StructureChain<RootStruct_T>::StructureChain(
    const StructureChain& other,
    const allocator_type& allocator
)
    : m_root_struct{ other.m_root_struct },
      m_chain{ other.m_chain, allocator }
{
    connect();
}

template <util::naked_c RootStruct_T>
StructureChain<RootStruct_T>::StructureChain(StructureChain&& other) noexcept
    : m_root_struct{ std::move(other.m_root_struct) },
      m_chain{ std::move(other.m_chain) }
{
    connect();
}

template <util::naked_c RootStruct_T>
StructureChain<RootStruct_T>::StructureChain(
    StructureChain&&      other,
    const allocator_type& allocator
)
    : m_root_struct{ std::move(other.m_root_struct) },
      m_chain{ std::move(other.m_chain), allocator }
{
    connect();
}

template <util::naked_c RootStruct_T>
StructureChain<RootStruct_T>::StructureChain(const allocator_type& allocator)
    : m_chain{ allocator }
{
}

template <util::naked_c RootStruct_T>
auto StructureChain<RootStruct_T>::operator=(const StructureChain& other)
    -> StructureChain&
{
    if (this == &other)
    {
        return *this;
    }

    m_root_struct = other.m_root_struct;
    m_chain       = other.m_chain;

    connect();

    return *this;
}

template <util::naked_c RootStruct_T>
auto StructureChain<RootStruct_T>::operator=(StructureChain&& other) noexcept
    -> StructureChain&
{
    if (this == &other)
    {
        return *this;
    }

    m_root_struct = std::move(other.m_root_struct);
    m_chain       = std::move(other.m_chain);

    connect();

    return *this;
}

template <util::naked_c RootStruct_T>
auto StructureChain<RootStruct_T>::get_allocator() const noexcept -> allocator_type
{
    return m_chain.keys().get_allocator();
}

template <util::naked_c RootStruct_T>
template <typename Self_T>
constexpr auto StructureChain<RootStruct_T>::root(this Self_T&& self)
    -> util::forward_like_t<RootStruct_T, Self_T>
{
    return std::forward_like<Self_T>(self.m_root_struct);
}

template <util::naked_c RootStruct_T>
template <extends_struct_c<RootStruct_T> Struct_T>
constexpr auto StructureChain<RootStruct_T>::contains() const -> bool
{
    return m_chain.contains(Struct_T::structureType);
}

template <util::naked_c RootStruct_T>
template <extends_struct_c<RootStruct_T> Struct_T, typename Self_T>
constexpr auto StructureChain<RootStruct_T>::find(this Self_T& self)
    -> util::OptionalRef<util::const_like_t<Struct_T, std::remove_reference_t<Self_T>>>
{
    const auto iter{ self.m_chain.find(Struct_T::structureType) };
    if (iter == self.m_chain.cend())
    {
        return std::nullopt;
    }

    return util::any_cast<StructureChainNode<RootStruct_T, Struct_T>>(iter->second)
        .structure();
}

template <util::naked_c RootStruct_T>
template <extends_struct_c<RootStruct_T> Struct_T, typename Self_T>
constexpr auto StructureChain<RootStruct_T>::at(this Self_T&& self)
    -> util::forward_like_t<Struct_T, Self_T>
{
    PRECOND(self.template contains<Struct_T>());

    return util::any_cast<StructureChainNode<RootStruct_T, Struct_T>>(
               std::forward_like<Self_T>(self.m_map.at(Struct_T::structureType))
    )
        .structure();
}

template <util::naked_c RootStruct_T>
constexpr auto StructureChain<RootStruct_T>::matches(
    const vk::PhysicalDeviceFeatures2& physical_device_features
) const -> bool
    requires std::same_as<RootStruct_T, vk::PhysicalDeviceFeatures2>
{
    if (!match_physical_device_features(
            m_root_struct.features,
            physical_device_features.features
        ))
    {
        return false;
    }

    for (
        const auto* supported_feature_struct{
            static_cast<const vk::BaseInStructure*>(physical_device_features.pNext) };
        supported_feature_struct != nullptr;
        supported_feature_struct = supported_feature_struct->pNext
    )
    {
        if (const auto iter = m_chain.find(supported_feature_struct->sType);
            iter != m_chain.cend()
            && !iter->second->is_subset_of(*supported_feature_struct))
        {
            return false;
        }
    }

    return true;
}

template <util::naked_c RootStruct_T>
template <extends_struct_c<vk::PhysicalDeviceFeatures2> Features_T>
constexpr auto StructureChain<RootStruct_T>::merge(const Features_T& features) -> void
    requires std::same_as<RootStruct_T, vk::PhysicalDeviceFeatures2>
{
    merge_physical_device_features(operator[](std::in_place_type<Features_T>), features);
}

template <util::naked_c RootStruct_T>
constexpr auto StructureChain<RootStruct_T>::merge(
    const StructureChain<vk::PhysicalDeviceFeatures2>& other
) -> void
    requires std::same_as<RootStruct_T, vk::PhysicalDeviceFeatures2>
{
    merge_physical_device_features(m_root_struct.features, other.m_root_struct.features);

    bool needs_reconnect{};
    for (const auto& [incoming_sType, incoming_features] : other.m_chain)
    {
        const auto [iter, inserted]
            = m_chain.try_emplace(incoming_sType, incoming_features);

        *iter->second
            |= *static_cast<const vk::BaseInStructure*>(incoming_features->address());

        needs_reconnect |= inserted;
    }

    if (needs_reconnect)
    {
        connect();
    }
}

template <util::naked_c RootStruct_T>
template <vulkan1x_feature_struct_c Vulkan1XFeatures_T>
constexpr auto
    StructureChain<RootStruct_T>::erase_and_merge_features_to_vulkan1x_feature_struct(
        Vulkan1XFeatures_T& vulkan1X_features
    ) -> void
    requires std::same_as<RootStruct_T, vk::PhysicalDeviceFeatures2>
{
    std::vector<vk::StructureType> marked_for_removal;

    for (const auto [sType, erased_features] : m_chain)
    {
        if (erased_features->try_merge_into(vulkan1X_features))
        {
            marked_for_removal.push_back(sType);
        }
    }

    // NOLINTNEXTLINE(*-identifier-naming)
    for (const vk::StructureType sType : marked_for_removal)
    {
        m_chain.erase(sType);
    }

    if (!marked_for_removal.empty())
    {
        connect();
    }
}

template <util::naked_c RootStruct_T>
constexpr auto StructureChain<RootStruct_T>::erase_unsupported_features(
    const vk::PhysicalDeviceFeatures2& supported_features
) -> void
    requires std::same_as<RootStruct_T, vk::PhysicalDeviceFeatures2>
{
    filter_physical_device_features(m_root_struct.features, supported_features.features);

    bool needs_reconnect{};

    for (
        const auto* incoming_feature_struct{
            static_cast<const vk::BaseInStructure*>(supported_features.pNext) };
        incoming_feature_struct != nullptr;
        incoming_feature_struct = incoming_feature_struct->pNext
    )
    {
        if (const auto iter = m_chain.find(incoming_feature_struct->sType);
            iter != m_chain.end())
        {
            *iter->second &= *incoming_feature_struct;
            if (!*iter->second)
            {
                m_chain.erase(iter);
                needs_reconnect |= true;
            }
        }
    }

    if (needs_reconnect)
    {
        connect();
    }
}

template <util::naked_c RootStruct_T>
constexpr auto StructureChain<RootStruct_T>::erase_features(
    const vk::PhysicalDeviceFeatures& features
) -> void
    requires std::same_as<RootStruct_T, vk::PhysicalDeviceFeatures2>
{
    erase_physical_device_features(m_root_struct.features, features);
}

template <util::naked_c RootStruct_T>
template <extends_struct_c<vk::PhysicalDeviceFeatures2> Features_T>
constexpr auto StructureChain<RootStruct_T>::erase_features(const Features_T& features)
    -> void
    requires std::same_as<RootStruct_T, vk::PhysicalDeviceFeatures2>
{
    if (const auto iter = m_chain.find(Features_T::structureType); iter != m_chain.cend())
    {
        *iter->second -= reinterpret_cast<const vk::BaseInStructure&>(features);
        if (!*iter->second)
        {
            m_chain.erase(iter);
            connect();
        }
    }
}

template <util::naked_c RootStruct_T>
constexpr auto StructureChain<RootStruct_T>::erase_features(
    const vk::PhysicalDeviceFeatures2& physical_device_features
) -> void
    requires std::same_as<RootStruct_T, vk::PhysicalDeviceFeatures2>
{
    erase_physical_device_features(
        m_root_struct.features,
        physical_device_features.features
    );

    bool needs_reconnect{};

    for (
        const auto* incoming_feature_struct{
            static_cast<const vk::BaseInStructure*>(physical_device_features.pNext) };
        incoming_feature_struct != nullptr;
        incoming_feature_struct = incoming_feature_struct->pNext
    )
    {
        if (const auto iter = m_chain.find(incoming_feature_struct->sType);
            iter != m_chain.end())
        {
            *iter->second -= *incoming_feature_struct;
            if (!*iter->second)
            {
                m_chain.erase(iter);
                needs_reconnect |= true;
            }
        }
    }

    if (needs_reconnect)
    {
        connect();
    }
}

template <util::naked_c RootStruct_T>
template <typename Self_T, extends_struct_c<RootStruct_T> Struct_T>
constexpr auto StructureChain<RootStruct_T>::operator[](
    this Self_T&& self,
    std::in_place_type_t<Struct_T>
) -> util::forward_like_t<Struct_T, Self_T>
{
    const auto [iter, inserted] = self.m_chain.try_emplace(
        Struct_T::structureType,
        std::in_place_type<StructureChainNode<RootStruct_T, Struct_T>>
    );

    if (inserted)
    {
        self.connect();
    }

    return util::any_cast<StructureChainNode<RootStruct_T, Struct_T>>(
               std::forward_like<Self_T>(iter->second)
    )
        .structure();
}

template <util::naked_c RootStruct_T>
constexpr auto StructureChain<RootStruct_T>::connect() -> void
{
    std::reference_wrapper<NextPtr> previous_pointer{ m_root_struct.pNext };
    for (ErasedStructureChainNode<RootStruct_T>& next_struct : std::views::values(m_chain))
    {
        previous_pointer.get() = next_struct->address();
        previous_pointer       = next_struct->next_pointer();
    }
    previous_pointer.get() = nullptr;
}

}   // namespace kiln::gfx::vulkan
