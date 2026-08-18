module;

#include <algorithm>
#include <limits>
#include <optional>
#include <span>
#include <variant>
#include <vector>

module kiln.gfx.renderer.presentation.Swapchain;

import vulkan;

import kiln.gfx.vulkan.result.check_result;
import kiln.gfx.vulkan.result.Result;
import kiln.gfx.vulkan.result.TypedResultCode;
import kiln.util.containers.OptionalRef;
import kiln.util.contracts;

namespace kiln::gfx::renderer {

constexpr auto pick_present_mode(
    const std::span<const vk::PresentModeKHR> present_modes,
    const bool                                vsync
) -> vk::PresentModeKHR
{
    if (vsync)
    {
        if (std::ranges::contains(present_modes, vk::PresentModeKHR::eImmediate))
        {
            return vk::PresentModeKHR::eImmediate;
        }
        if (std::ranges::contains(present_modes, vk::PresentModeKHR::eFifoRelaxed))
        {
            return vk::PresentModeKHR::eFifoRelaxed;
        }
    }

    return std::ranges::contains(present_modes, vk::PresentModeKHR::eMailbox)
             ? vk::PresentModeKHR::eMailbox
             : vk::PresentModeKHR::eFifo;
}

[[nodiscard]]
auto pick_swap_extent(
    const vk::Extent2D&               framebuffer_size,
    const vk::SurfaceCapabilitiesKHR& surface_capabilities
) -> vk::Extent2D
{
    if (surface_capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return surface_capabilities.currentExtent;
    }

    return vk::Extent2D{
        .width = std::clamp(
            static_cast<uint32_t>(framebuffer_size.width),
            surface_capabilities.minImageExtent.width,
            surface_capabilities.maxImageExtent.width
        ),
        .height = std::clamp(
            static_cast<uint32_t>(framebuffer_size.height),
            surface_capabilities.minImageExtent.height,
            surface_capabilities.maxImageExtent.height
        ),
    };
}

[[nodiscard]]
auto create_swapchain(
    const Device&                     device,
    const vk::raii::SurfaceKHR&       surface,
    const vk::SurfaceCapabilitiesKHR& surface_capabilities,
    const vk::Extent2D&               extent,
    const uint32_t                    number_of_frames,
    const bool                        vsync,
    const vk::SurfaceFormatKHR&       surface_format,
    const Swapchain*                  old_swapchain
) -> vk::raii::SwapchainKHR
{
    const vk::PresentModeKHR present_mode{
        pick_present_mode(
            vulkan::check_result(
                device.physical_device().getSurfacePresentModesKHR(surface)   //
            ),
            vsync
        ),
    };
    const uint32_t image_count{
        surface_capabilities.maxImageCount == 0
            ? std::max(number_of_frames, surface_capabilities.minImageCount)
            : std::min(
                  std::max(number_of_frames, surface_capabilities.minImageCount),
                  surface_capabilities.maxImageCount
              )
    };

    const vk::SwapchainCreateInfoKHR create_info{
        .surface          = surface,
        .minImageCount    = image_count,
        .imageFormat      = surface_format.format,
        .imageColorSpace  = surface_format.colorSpace,
        .imageExtent      = extent,
        .imageArrayLayers = 1,
        .imageUsage       = vk::ImageUsageFlagBits::eColorAttachment,
        .imageSharingMode = vk::SharingMode::eExclusive,
        .preTransform     = surface_capabilities.currentTransform,
        .compositeAlpha   = vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode      = present_mode,
        .clipped          = vk::True,
        .oldSwapchain     = old_swapchain == nullptr ? vk::SwapchainKHR{}
                                                     : old_swapchain->get(),
    };

    return vulkan::check_result(device.logical_device().createSwapchainKHR(create_info));
}

[[nodiscard]]
auto create_swapchain_image_views(
    const vk::raii::Device&          device,
    const vk::Format                 swapchain_image_format,
    const std::span<const vk::Image> swapchain_images
) -> std::vector<vk::raii::ImageView>
{
    std::vector<vk::raii::ImageView> result;
    result.reserve(swapchain_images.size());

    constexpr static vk::ImageSubresourceRange subresource_range{
        .aspectMask     = vk::ImageAspectFlagBits::eColor,
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = 1,
    };
    vk::ImageViewCreateInfo create_info{
        .viewType         = vk::ImageViewType::e2D,
        .format           = swapchain_image_format,
        .subresourceRange = subresource_range,
    };

    for (const vk::Image swapchain_image : swapchain_images)
    {
        create_info.image = swapchain_image;
        result.push_back(vulkan::check_result(device.createImageView(create_info)));
    }

    return result;
}

Swapchain::Swapchain(
    const vk::raii::SurfaceKHR& surface,
    const vk::SurfaceFormatKHR& surface_format,
    const Device&               device,
    const vk::Extent2D          framebuffer_size,
    const uint32_t              number_of_frames,
    const bool                  enable_vsync,
    const Swapchain*            old_swapchain
)
    : m_surface_capabilities{
          vulkan::check_result(
              device.physical_device().getSurfaceCapabilitiesKHR(surface)
          ),
      },
      m_extent{
          pick_swap_extent(framebuffer_size, m_surface_capabilities),
      },
      m_swapchain{
          create_swapchain(
              device,
              surface,
              m_surface_capabilities,
              m_extent,
              number_of_frames,
              enable_vsync,
              surface_format,
              old_swapchain
          ),
      },
      m_swapchain_images{ vulkan::check_result(m_swapchain.getImages()) },
      m_swapchain_image_views{
          create_swapchain_image_views(
              device.logical_device(),
              surface_format.format,
              m_swapchain_images
          ),
      }
{
}

auto Swapchain::get() const noexcept -> const vk::raii::SwapchainKHR&
{
    return m_swapchain;
}

auto Swapchain::extent() const noexcept -> vk::Extent2D
{
    return m_extent;
}

auto Swapchain::number_of_images() const noexcept -> uint32_t
{
    return static_cast<uint32_t>(m_swapchain_images.size());
}

auto Swapchain::image_at(const uint32_t index) const noexcept -> const vk::Image&
{
    return m_swapchain_images[index];
}

auto Swapchain::image_view_at(const uint32_t index) const noexcept
    -> const vk::raii::ImageView&
{
    return m_swapchain_image_views[index];
}

auto Swapchain::acquire_next_image_index(
    const util::OptionalRef<const vk::raii::Semaphore> signal_semaphore,
    const util::OptionalRef<const vk::raii::Fence>     fence
) -> std::optional<uint32_t>
{
    if (m_out_dated)
    {
        return std::nullopt;
    }

    const vulkan::Result<
        uint32_t,
        vk::Result::eSuccess,
        vk::Result::eSuboptimalKHR,
        vk::Result::eErrorOutOfDateKHR>
        image_index_result
        = vulkan::check_result<vk::Result::eSuboptimalKHR, vk::Result::eErrorOutOfDateKHR>(
            m_swapchain.acquireNextImage(
                std::numeric_limits<uint64_t>::max(),
                signal_semaphore.has_value() ? *signal_semaphore : vk::Semaphore{},
                fence.has_value() ? *fence : vk::Fence{}
            )
        );

    if (std::holds_alternative<vulkan::TypedResultCode<vk::Result::eErrorOutOfDateKHR>>(
            image_index_result.result_code()
        ))
    {
        m_out_dated = true;
        return std::nullopt;
    }

    return *image_index_result;
}

auto Swapchain::present(
    const PresentQueueRef&               queue,
    const uint32_t                       image_index,
    const std::span<const vk::Semaphore> wait_semaphores
) -> bool
{
    if (m_out_dated)
    {
        return false;
    }

    const vk::PresentInfoKHR present_info{
        .waitSemaphoreCount = static_cast<uint32_t>(wait_semaphores.size()),
        .pWaitSemaphores    = wait_semaphores.data(),
        .swapchainCount     = 1,
        .pSwapchains        = &*m_swapchain,
        .pImageIndices      = &image_index,
    };

    if (const std::variant result = queue.present(present_info);
        std::holds_alternative<vulkan::TypedResultCode<vk::Result::eErrorOutOfDateKHR>>(
            result
        ))
    {
        m_out_dated = true;
        return false;
    }

    return true;
}

}   // namespace kiln::gfx::renderer
