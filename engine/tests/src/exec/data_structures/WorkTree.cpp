#include <array>
#include <atomic>
#include <expected>
#include <ranges>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

import kiln.exec_old.data_structures.ReleaseWorkContract;
import kiln.exec_old.data_structures.WorkContract;
import kiln.exec_old.data_structures.WorkID;
import kiln.exec_old.data_structures.WorkContinuation;
import kiln.exec_old.data_structures.WorkTree;
import kiln.util.contracts;
import kiln.util.reflection;

namespace kiln::exec {

namespace {

const std::string test_name{ util::name_of<WorkTree>() };

}   // namespace

TEST_CASE(test_name)
{
    WorkTree work_tree{ 256, 3 };

    SECTION("empty")
    {
        const bool executed = work_tree.try_execute_one_work(0);
        REQUIRE(executed == false);
    }

    SECTION("single work")
    {
        bool                                executed{};
        std::expected<WorkID, WorkContract> work_id{
            work_tree.try_emplace(
                WorkContract{
                    [&executed]
                    {
                        executed = true;
                        return WorkContinuation::eRelease;
                    },
                }
            ),
        };

        REQUIRE(work_tree.try_execute_one_work(0) == false);

        work_tree.schedule(*work_id);

        REQUIRE(work_tree.try_execute_one_work(0) == true);
        REQUIRE(executed == true);

        REQUIRE(work_tree.try_execute_one_work(0) == false);
    }

    SECTION("reschedule")
    {
        std::expected<WorkID, WorkContract> work_id{
            work_tree.try_emplace(
                WorkContract{
                    [rescheduled = false] mutable
                    {
                        if (!rescheduled)
                        {
                            rescheduled = true;
                            return WorkContinuation::eReschedule;
                        }
                        return WorkContinuation::eRelease;
                    },
                }
            ),
        };

        REQUIRE(work_tree.try_execute_one_work(0) == false);

        work_tree.schedule(*work_id);

        REQUIRE(work_tree.try_execute_one_work(0) == true);
        REQUIRE(work_tree.try_execute_one_work(0) == true);

        REQUIRE(work_tree.try_execute_one_work(0) == false);
    }

    SECTION("release")
    {
        bool                                                                released{};
        std::expected<WorkID, std::pair<WorkContract, ReleaseWorkContract>> work_id{
            work_tree.try_emplace(
                WorkContract{ [] { return WorkContinuation::eRelease; } },
                ReleaseWorkContract{ [&released] { released = true; } }
            )
        };

        REQUIRE(work_tree.try_execute_one_work(0) == false);

        work_tree.schedule(*work_id);

        REQUIRE(work_tree.try_execute_one_work(0) == true);
        REQUIRE(released == false);

        REQUIRE(work_tree.try_execute_one_work(0) == true);
        REQUIRE(released == true);

        REQUIRE(work_tree.try_execute_one_work(0) == false);
    }

    SECTION("fairness")
    {
        WorkTree local_work_tree{ 256, 1 };

        int executed_index{ -1 };

        const auto make_work_contract{
            [&executed_index](const int index) -> WorkContract
            {
                return WorkContract{
                    [&executed_index, index] -> WorkContinuation
                    {
                        executed_index = index;
                        return WorkContinuation::eDontCare;
                    },
                };
            },
        };

        const std::array work_ids{
            *local_work_tree.try_emplace(make_work_contract(0)),
            *local_work_tree.try_emplace(make_work_contract(1)),
            *local_work_tree.try_emplace(make_work_contract(2)),
            *local_work_tree.try_emplace(make_work_contract(3)),
        };

        for (const auto work_id : work_ids)
        {
            local_work_tree.schedule(work_id);
        }

        for (std::size_t i{}; i < work_ids.size(); ++i)
        {
            const bool success = local_work_tree.try_execute_one_work(0);
            REQUIRE(success);
            REQUIRE(executed_index == i);
        }
    }

    SECTION("simple multithreading")
    {
        constexpr static uint32_t work_count{ 256 };
        constexpr static uint32_t per_work_count{ 4 };
        constexpr static uint32_t goal{ work_count * per_work_count };
        constexpr static uint32_t thread_count{ 8 };
        std::atomic_uint32_t      counter;

        const auto process_work = [&work_tree, &counter](const uint32_t thread_id)
        {
            return [&work_tree, &counter, thread_id]
            {
                while (counter < goal)
                {
                    work_tree.try_execute_one_work(
                        thread_id % work_tree.optimized_for_thread_count()
                    );
                }
            };
        };

        for (const auto _ : std::views::repeat(std::ignore, work_count))
        {
            const WorkID work_id = *work_tree.try_emplace(
                WorkContract{
                    [&counter, local_counter = 0u] mutable
                    {
                        if (local_counter++ < per_work_count)
                        {
                            ++counter;
                            return WorkContinuation::eReschedule;
                        }
                        return WorkContinuation::eRelease;
                    },
                }
            );

            work_tree.schedule(work_id);
        }

        std::vector<std::jthread> threads;
        threads.reserve(thread_count);
        for (const uint32_t thread_id : std::views::iota(0u, thread_count))
        {
            threads.emplace_back(process_work(thread_id));
        }

        for (std::jthread& thread : threads)
        {
            thread.join();
        }

        REQUIRE(counter == goal);
    }
}

}   // namespace kiln::exec
