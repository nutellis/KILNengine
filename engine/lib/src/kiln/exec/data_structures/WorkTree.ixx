module;

#include <atomic>
#include <expected>
#include <memory>
#include <memory_resource>
#include <optional>
#include <utility>
#include <vector>

#include "kiln/util/contract_macros.hpp"

export module kiln.exec.data_structures.WorkTree;

import kiln.exec.data_structures.ReleaseWorkContract;
import kiln.exec.data_structures.SignalTree;
import kiln.exec.data_structures.WorkContract;
import kiln.exec.data_structures.WorkContinuation;
import kiln.exec.data_structures.WorkID;
import kiln.reg.BuildDirector;
import kiln.reg.EntryTraits;
import kiln.util.containers.MoveOnlyFunction;
import kiln.util.contracts;

namespace kiln::exec {

struct WorkFlags {
    using type = uint8_t;

    constexpr static type eNone              = 0;
    constexpr static type eActive            = 0x1u << 0u;
    constexpr static type eShouldBeScheduled = 0x1u << 1u;
    constexpr static type eShouldBeReleased  = 0x1u << 2u;
    constexpr static type eAll               = static_cast<type>(~eNone);
};

class WorkContractSlot {
public:
    auto assign(WorkContract&& work, std::optional<ReleaseWorkContract>&& release)
        -> WorkContractSlot&;

    [[nodiscard]]
    auto execute() -> WorkContinuation;

    [[nodiscard]]
    auto schedule() -> bool;
    [[nodiscard]]
    auto schedule_release() -> WorkContinuation;

private:
    std::atomic<WorkFlags::type>       m_flags;
    std::optional<WorkContract>        m_work;
    std::optional<ReleaseWorkContract> m_release;

    auto release() -> void;

    static_assert(std::remove_cvref_t<decltype(m_flags)>::is_always_lock_free);
};

struct ConvertingConstructorTag {};

export class WorkTree {
public:
    using allocator_type = std::pmr::polymorphic_allocator<>;


    /*
     * Conversion initialization using allocator
     */
    template <typename U>
        requires(!std::is_base_of_v<WorkTree, std::remove_reference_t<U>>)
             && std::is_convertible_v<U, WorkTree>
    WorkTree(U&&, const allocator_type&);

    explicit WorkTree(uint64_t capacity, uint32_t number_of_threads);
    explicit WorkTree(
        std::allocator_arg_t,
        const allocator_type& allocator,
        uint64_t              capacity,
        uint32_t              number_of_threads
    );


    [[nodiscard]]
    auto get_allocator() const noexcept -> allocator_type;

    [[nodiscard]]
    auto capacity() const noexcept -> uint64_t;
    [[nodiscard]]
    auto optimized_for_thread_count() const noexcept -> uint32_t;


    [[nodiscard]]
    auto try_emplace(WorkContract&& work) -> std::expected<WorkID, WorkContract>;
    [[nodiscard]]
    auto try_emplace(WorkContract&& work, ReleaseWorkContract&& release)
        -> std::expected<WorkID, std::pair<WorkContract, ReleaseWorkContract>>;

    [[nodiscard]]
    auto try_emplace_at(WorkID work_id, WorkContract&& work)
        -> std::expected<void, WorkContract>;
    [[nodiscard]]
    auto try_emplace_at(WorkID work_id, WorkContract&& work, ReleaseWorkContract&& release)
        -> std::expected<void, std::pair<WorkContract, ReleaseWorkContract>>;


    auto schedule(WorkID work_id) -> void;
    auto schedule_for_release(WorkID work_id) -> void;


    auto try_execute_one_work(uint32_t thread_index) -> bool;

private:
    std::pmr::vector<SignalTree>            m_free_signals;
    std::pmr::vector<SignalTree>            m_contract_signals;
    std::pmr::vector<WorkContractSlot>      m_work_contracts_slots;
    std::atomic<uint32_t>                   m_next_available_sub_tree_index{};
    std::pmr::vector<std::atomic<uint32_t>> m_per_thread_emplace_strategy_index;
    std::pmr::vector<std::atomic<uint32_t>> m_per_thread_execute_strategy_index;


    explicit WorkTree(ConvertingConstructorTag, WorkTree&&, const allocator_type&);

    [[nodiscard]]
    auto try_emplace(
        WorkContract&&                       work,
        std::optional<ReleaseWorkContract>&& release
    ) -> std::expected<WorkID, std::pair<WorkContract, std::optional<ReleaseWorkContract>>>;
    [[nodiscard]]
    auto try_emplace_at(
        WorkID                               work_id,
        WorkContract&&                       work,
        std::optional<ReleaseWorkContract>&& release
    ) -> std::expected<void, std::pair<WorkContract, std::optional<ReleaseWorkContract>>>;


    auto handle_work_result(WorkID work_id, WorkContinuation work_continuation) -> void;


    static_assert(
        std::remove_cvref_t<decltype(m_next_available_sub_tree_index)>::is_always_lock_free
    );
};

}   // namespace kiln::exec

namespace kiln::exec {

template <typename U>
    requires(!std::is_base_of_v<WorkTree, std::remove_reference_t<U>>)
         && std::is_convertible_v<U, WorkTree>
WorkTree::WorkTree(U&& other, const allocator_type& allocator)
    : WorkTree{ ConvertingConstructorTag{}, std::forward<U>(other), allocator }
{
    PRECOND(get_allocator() == allocator);
}

}   // namespace kiln::exec

template <>
struct kiln::reg::EntryTraits<kiln::exec::WorkTree> {
    static auto describe_build(BuildDirector<exec::WorkTree>& build_director) -> void;
};
