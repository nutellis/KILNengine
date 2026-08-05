module;

#include <utility>

export module kiln.gfx.vulkan.structure_chain.filter_physical_device_features;

import vulkan;

import kiln.gfx.vulkan.structure_chain.feature_struct_c;

namespace kiln::gfx::vulkan {

export template <feature_struct_c FeatureStruct_T>
constexpr auto filter_physical_device_features(
    FeatureStruct_T&       inout,
    const FeatureStruct_T& in
) -> void;

}   // namespace kiln::gfx::vulkan

namespace kiln::gfx::vulkan {

constexpr auto filter_feature_struct_member(
    const vk::StructureType,
    const vk::StructureType
) -> void
{
}

constexpr auto filter_feature_struct_member(void* const, void* const) -> void {}

constexpr auto filter_feature_struct_member(vk::Bool32& inout, const vk::Bool32 in)
    -> void
{
    inout &= in;
}

template <feature_struct_c FeatureStruct_T>
constexpr auto filter_physical_device_features(
    FeatureStruct_T&       inout,
    const FeatureStruct_T& in
) -> void
{
    auto& [... inout_members]{ inout };
    const auto& [... in_members]{ in };
    (filter_feature_struct_member(inout_members, in_members), ...);
}

}   // namespace kiln::gfx::vulkan
