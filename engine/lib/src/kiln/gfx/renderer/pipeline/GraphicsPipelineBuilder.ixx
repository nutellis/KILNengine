module;

#include <functional>
#include <span>
#include <utility>

#include "kiln/util/memory/lifetimebound.hpp"

export module kiln.gfx.renderer.pipeline.GraphicsPipelineBuilder;

import vulkan;

import kiln.gfx.renderer.device.Device;
import kiln.gfx.renderer.pipeline.GraphicsPipeline;
import kiln.gfx.renderer.pipeline.ShaderModule;

namespace kiln::gfx::renderer {

export class GraphicsPipelineBuilder {
public:
    // clang-format off
    explicit GraphicsPipelineBuilder(
        kiln_lifetimebound const vk::raii::PipelineLayout& layout,
        kiln_lifetimebound const ShaderModule&             vertex_shader_module,
        kiln_lifetimebound const ShaderModule&             fragment_shader_module
    );
    // clang-format on


    [[nodiscard]]
    auto build(const Device& device) const -> GraphicsPipeline;

    template <typename Self_T>
    auto set_cull_mode(this Self_T&& self, vk::CullModeFlags cull_mode) -> Self_T&&;
    template <typename Self_T>
    auto set_color_formats(
        this Self_T&&      self,
        kiln_lifetimebound std::span<const vk::Format> formats
    ) -> Self_T&&;
    template <typename Self_T>
    auto set_depth_format(this Self_T&& self, vk::Format format) -> Self_T&&;

private:
    std::reference_wrapper<const vk::raii::PipelineLayout> m_layout;
    std::reference_wrapper<const ShaderModule>             m_vertex_shader_module;
    std::reference_wrapper<const ShaderModule>             m_fragment_shader_module;
    vk::CullModeFlags                                      m_cull_mode;
    std::span<const vk::Format>                            m_color_formats;
    vk::Format m_depth_format{ vk::Format::eUndefined };
};

}   // namespace kiln::gfx::renderer

namespace kiln::gfx::renderer {

template <typename Self_T>
auto GraphicsPipelineBuilder::set_cull_mode(
    this Self_T&&           self,
    const vk::CullModeFlags cull_mode
) -> Self_T&&
{
    self.m_cull_mode = cull_mode;
    return std::forward<Self_T>(self);
}

template <typename Self_T>
auto GraphicsPipelineBuilder::set_color_formats(
    this Self_T&&                     self,
    const std::span<const vk::Format> formats
) -> Self_T&&
{
    self.m_color_formats = formats;
    return std::forward<Self_T>(self);
}

template <typename Self_T>
auto GraphicsPipelineBuilder::set_depth_format(this Self_T&& self, const vk::Format format)
    -> Self_T&&
{
    self.m_depth_format = format;
    return std::forward<Self_T>(self);
}

}   // namespace kiln::gfx::renderer
