module;

#include <memory>

export module kiln.util.memory.uses_allocator_construction_args;

import kiln.util.containers.Tuple;

namespace kiln::util {

export template <typename T, typename Allocator_T, typename... Args_T>
constexpr auto uses_allocator_construction_args(
    const Allocator_T& allocator,
    Args_T&&... args
)
{
    if constexpr (!std::uses_allocator_v<T, Allocator_T>)
    {
        return forward_as_tuple(std::forward<Args_T>(args)...);
    }
    else if constexpr (
        std::is_constructible_v<T, std::allocator_arg_t, const Allocator_T&, Args_T...>
    )
    {
        return forward_as_tuple(
            std::allocator_arg,
            allocator,
            std::forward<Args_T>(args)...
        );
    }
    else if constexpr (std::is_constructible_v<T, Args_T..., const Allocator_T&>)
    {
        return forward_as_tuple(std::forward<Args_T>(args)..., allocator);
    }
    else
    {
        static_assert(false, "invalid `uses_allocator` specialization");
    }
}

}   // namespace kiln::util
