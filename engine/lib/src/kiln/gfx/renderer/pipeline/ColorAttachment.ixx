module;

#include <array>
#include <functional>
#include <optional>
#include <span>
#include <utility>

#include "kiln/util/memory/lifetimebound.hpp"

export module kiln.gfx.renderer.pipeline.ColorAttachment;

import vulkan;

namespace kiln::gfx::renderer {

export class ColorAttachment {
public:
    // clang-format off
    explicit ColorAttachment(kiln_lifetimebound const vk::raii::ImageView& image_view);
    // clang-format on

    [[nodiscard]]
    auto image_view() const noexcept -> const vk::raii::ImageView&;
    [[nodiscard]]
    auto clear_value() const noexcept kiln_lifetimebound
        -> std::optional<std::span<const float, 4>>;

    template <typename Self_T>
    auto set_clear_value(this Self_T&&, std::span<const float, 4> value) noexcept
        -> Self_T&&;

private:
    std::reference_wrapper<const vk::raii::ImageView> m_image_view;
    std::optional<std::array<float, 4>>               m_clear_value;
};

}   // namespace kiln::gfx::renderer

namespace kiln::gfx::renderer {

ColorAttachment::ColorAttachment(const vk::raii::ImageView& image_view)
    : m_image_view{ image_view }
{
}

auto ColorAttachment::image_view() const noexcept -> const vk::raii::ImageView&
{
    return m_image_view;
}

auto ColorAttachment::clear_value() const noexcept
    -> std::optional<std::span<const float, 4>>
{
    return m_clear_value.transform(
        [](const std::array<float, 4>& value) -> std::span<const float, 4>
        {
            return value;   //
        }
    );
}

template <typename Self_T>
auto ColorAttachment::set_clear_value(
    this Self_T&&                   self,
    const std::span<const float, 4> value
) noexcept -> Self_T&&
{
    self.ColorAttachment::m_clear_value = std::array{
        value[0],
        value[1],
        value[2],
        value[3],
    };
    return std::forward<Self_T>(self);
}

}   // namespace kiln::gfx::renderer
