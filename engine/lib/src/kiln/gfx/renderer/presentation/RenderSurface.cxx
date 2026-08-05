module;

#include <optional>
#include <span>
#include <utility>

module kiln.gfx.renderer.presentation.RenderSurface;

import kiln.gfx.renderer.device.Device;
import kiln.gfx.renderer.presentation.Swapchain;
import kiln.gfx.vulkan.result.check_result;

namespace kiln::gfx::renderer {

[[nodiscard]]
constexpr auto pick_surface_format(
    const std::span<const vk::SurfaceFormatKHR> surface_formats
) -> vk::SurfaceFormatKHR
{
    for (const vk::SurfaceFormatKHR& surface_format : surface_formats)
    {
        if (surface_format.format == vk::Format::eB8G8R8A8Srgb
            && surface_format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
        {
            return surface_format;
        }
    }
    return surface_formats.front();
}

RenderSurface::RenderSurface(
    vk::raii::SurfaceKHR&& surface,
    const Device&          device,
    const uint8_t          number_of_frames_in_flight,
    const bool             vsync,
    const vk::Extent2D     size
)
    : m_surface{ std::move(surface) },
      m_device_ref{ device },
      m_surface_format{ pick_surface_format(
          vulkan::check_result(device.physical_device().getSurfaceFormatsKHR(m_surface))
      ) },
      m_number_of_frames_in_flight{ number_of_frames_in_flight },
      m_vsync{ vsync }
{
    resize(size);
}

auto RenderSurface::surface_format() const noexcept -> const vk::SurfaceFormatKHR&
{
    return m_surface_format;
}

auto RenderSurface::extent() const noexcept -> std::optional<vk::Extent2D>
{
    return m_swapchain.transform(&Swapchain::extent);
}

auto RenderSurface::number_of_images() const noexcept -> uint32_t
{
    return m_swapchain.transform(&Swapchain::number_of_images).value_or(0);
}

auto RenderSurface::image_at(const uint32_t index) const noexcept -> const vk::Image&
{
    return m_swapchain->image_at(index);
}

auto RenderSurface::image_view_at(const uint32_t index) const noexcept
    -> const vk::raii::ImageView&
{
    return m_swapchain->image_view_at(index);
}

auto RenderSurface::resize(const vk::Extent2D size) -> void
{
    if (extent() == size)
    {
        return;
    }

    if (size.width != 0 && size.height != 0)
    {
        m_swapchain = Swapchain{
            m_surface,
            m_surface_format,
            m_device_ref,
            size,
            m_number_of_frames_in_flight,
            m_vsync,
            m_swapchain
                .transform(
                    [](const Swapchain& old_swapchain) -> const Swapchain*
                    {
                        return &old_swapchain;   //
                    }
                )
                .value_or(nullptr),
        };
    }
}

auto RenderSurface::acquire_image(
    const util::OptionalRef<const vk::raii::Semaphore> signal_semaphore,
    const util::OptionalRef<const vk::raii::Fence>     fence
) -> std::optional<uint32_t>
{
    return m_swapchain.and_then(
        [signal_semaphore, fence](Swapchain& swapchain) -> std::optional<uint32_t>
        {
            return swapchain.acquire_next_image_index(signal_semaphore, fence);   //
        }
    );
}

auto RenderSurface::present(
    const PresentQueueRef&               queue,
    const uint32_t                       image_index,
    const std::span<const vk::Semaphore> wait_semaphores
) -> bool
{
    if (!m_swapchain.has_value())
    {
        return false;
    }

    return m_swapchain->present(queue, image_index, wait_semaphores);
}

}   // namespace kiln::gfx::renderer
