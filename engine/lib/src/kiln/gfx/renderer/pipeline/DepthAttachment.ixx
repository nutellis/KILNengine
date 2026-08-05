module;

#include <optional>
#include <utility>

#include "kiln/util/memory/lifetimebound.hpp"

export module kiln.gfx.renderer.pipeline.DepthAttachment;

import vulkan;

namespace kiln::gfx::renderer {

export class DepthAttachment {
public:
    // clang-format off
    explicit DepthAttachment(kiln_lifetimebound const vk::raii::ImageView& image_view);
    // clang-format on

    [[nodiscard]]
    auto image_view() const noexcept -> const vk::raii::ImageView&;
    [[nodiscard]]
    auto clear_value() const noexcept -> std::optional<float>;

    template <typename Self_T>
    auto set_clear_value(this Self_T&&, float value) noexcept -> Self_T&&;

private:
    std::reference_wrapper<const vk::raii::ImageView> m_image_view;
    std::optional<float>                              m_clear_value;
};

}   // namespace kiln::gfx::renderer

namespace kiln::gfx::renderer {

DepthAttachment::DepthAttachment(const vk::raii::ImageView& image_view)
    : m_image_view{ image_view }
{
}

auto DepthAttachment::image_view() const noexcept -> const vk::raii::ImageView&
{
    return m_image_view;
}

auto DepthAttachment::clear_value() const noexcept -> std::optional<float>
{
    return m_clear_value;
}

template <typename Self_T>
auto DepthAttachment::set_clear_value(this Self_T&& self, const float value) noexcept
    -> Self_T&&
{
    self.DepthAttachment::m_clear_value = value;
    return std::forward<Self_T>(self);
}

}   // namespace kiln::gfx::renderer
