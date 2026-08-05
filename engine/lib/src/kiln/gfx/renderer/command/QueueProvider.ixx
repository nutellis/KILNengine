module;

#include <array>
#include <concepts>
#include <cstdint>
#include <functional>
#include <optional>
#include <type_traits>

#include <magic_enum/magic_enum.hpp>

#include "kiln/util/memory/lifetimebound.hpp"

export module kiln.gfx.renderer.command.QueueProvider;

import vulkan;

import kiln.gfx.renderer.command.ComputeQueueRef;
import kiln.gfx.renderer.command.GraphicsQueueRef;
import kiln.gfx.renderer.command.TransferQueueRef;
import kiln.gfx.renderer.command.Queue;
import kiln.gfx.renderer.command.QueueRef;
import kiln.gfx.renderer.command.QueueRefBase;
import kiln.gfx.renderer.command.QueueType;
import kiln.reg.BuildDirector;
import kiln.reg.EntryTraits;

namespace kiln::gfx::renderer {

struct QueueIndices {
    std::optional<uint32_t> graphics_queue_index;
    std::optional<uint32_t> compute_queue_index;
    std::optional<uint32_t> host_to_device_transfer_queue_index;

    [[nodiscard]]
    auto at(QueueType type) noexcept -> std::optional<uint32_t>&;
    [[nodiscard]]
    auto at(QueueType type) const noexcept -> const std::optional<uint32_t>&;
};

class QueueProviderPrecondition {
public:
    explicit QueueProviderPrecondition(
        const std::array<std::optional<Queue>, 3>& queues,
        const QueueIndices&                        queue_indices
    );
};

export class QueueProvider : private QueueProviderPrecondition {
public:
    using QueueIndices = QueueIndices;

    explicit QueueProvider(
        std::array<std::optional<Queue>, 3>&& queues,
        const QueueIndices&                   queue_indices
    );


    [[nodiscard]]
    auto graphics_queue() noexcept kiln_lifetimebound -> std::optional<GraphicsQueueRef>;
    [[nodiscard]]
    auto compute_queue() noexcept kiln_lifetimebound -> std::optional<ComputeQueueRef>;
    [[nodiscard]]
    auto host_to_device_transfer_queue() noexcept kiln_lifetimebound
        -> std::optional<TransferQueueRef>;

    template <std::derived_from<QueueRefBase> QueueRef_T>
    [[nodiscard]]
    auto graphics_queue_as() kiln_lifetimebound -> std::optional<QueueRef_T>;
    template <std::derived_from<QueueRefBase> QueueRef_T>
    [[nodiscard]]
    auto compute_queue_as() kiln_lifetimebound -> std::optional<QueueRef_T>;
    template <std::derived_from<QueueRefBase> QueueRef_T>
    [[nodiscard]]
    auto host_to_device_transfer_queue_as() kiln_lifetimebound
        -> std::optional<QueueRef_T>;

    template <typename RankFunc_T>
        requires std::same_as<
            std::invoke_result_t<RankFunc_T, const Queue&, QueueType>,
            std::optional<uint32_t>>
    [[nodiscard]]
    auto select_queue(RankFunc_T&& rank_func) -> std::optional<QueueRef>;

private:
    // TODO: use inplace_vector
    std::array<std::optional<Queue>, 3> m_queues;
    QueueIndices                        m_queue_indices;


    template <std::derived_from<QueueRefBase> QueueRef_T>
    [[nodiscard]]
    auto queue_as(std::optional<uint32_t> index) -> std::optional<QueueRef_T>;
};

}   // namespace kiln::gfx::renderer

template <>
struct kiln::reg::EntryTraits<kiln::gfx::renderer::QueueProvider> {
    static auto describe_build(
        BuildDirector<gfx::renderer::QueueProvider>& build_director
    ) -> void;
};

namespace kiln::gfx::renderer {

template <std::derived_from<QueueRefBase> QueueRef_T>
auto QueueProvider::graphics_queue_as() -> std::optional<QueueRef_T>
{
    return queue_as<QueueRef_T>(m_queue_indices.graphics_queue_index);
}

template <std::derived_from<QueueRefBase> QueueRef_T>
auto QueueProvider::compute_queue_as() -> std::optional<QueueRef_T>
{
    return queue_as<QueueRef_T>(m_queue_indices.compute_queue_index);
}

template <std::derived_from<QueueRefBase> QueueRef_T>
auto QueueProvider::host_to_device_transfer_queue_as() -> std::optional<QueueRef_T>
{
    return queue_as<QueueRef_T>(m_queue_indices.host_to_device_transfer_queue_index);
}

template <typename RankFunc_T>
    requires std::same_as<
        std::invoke_result_t<RankFunc_T, const Queue&, QueueType>,
        std::optional<uint32_t>>
auto QueueProvider::select_queue(RankFunc_T&& rank_func) -> std::optional<QueueRef>
{
    std::optional<QueueRef> result;
    std::optional<uint32_t> highest_rank;
    for (const QueueType queue_type : magic_enum::enum_values<QueueType>())
    {
        if (!m_queue_indices.at(queue_type).has_value())
        {
            break;
        }

        Queue& queue{ *m_queues[*m_queue_indices.at(queue_type)] };
        if (const std::optional<uint32_t> rank{
                std::invoke(rank_func, queue, queue_type) };
            rank.has_value() && (!highest_rank.has_value() || *rank > *highest_rank))
        {
            result.emplace(queue);
            highest_rank = rank;
        }
    }

    return result;
}

template <std::derived_from<QueueRefBase> QueueRef_T>
auto QueueProvider::queue_as(const std::optional<uint32_t> index)
    -> std::optional<QueueRef_T>
{
    if (index.has_value())
    {
        return QueueRef_T{ *m_queues[*index] };
    }
    return std::nullopt;
}

}   // namespace kiln::gfx::renderer
