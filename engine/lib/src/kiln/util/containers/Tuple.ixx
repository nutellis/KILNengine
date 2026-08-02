module;

#include <concepts>
#include <cstddef>
#include <type_traits>
#include <utility>

export module kiln.util.containers.Tuple;

import kiln.util.concepts.contained_exactly_once;
import kiln.util.type_traits.forward_like;

namespace kiln::util {

namespace internal {

template <typename T, typename... Ts, std::size_t... indices_T>
[[nodiscard]]
consteval auto index_of(std::index_sequence<indices_T...>) -> std::size_t
{
    std::size_t result;
    ((std::is_same_v<T, Ts> ? result = indices_T : 0), ...);
    return result;
}

template <typename T, typename... Ts>
inline constexpr std::size_t index_of_v{
    index_of<T, Ts...>(std::make_index_sequence<sizeof...(Ts)>{})
};

template <std::size_t, typename T>
struct Node {
    T value{};
};

template <typename IndexSequence_T, typename... Ts>
class TupleImpl;

template <std::size_t... indices_T, typename... Ts>
class TupleImpl<std::index_sequence<indices_T...>, Ts...>
    : private Node<indices_T, Ts>...   //
{
public:
    explicit TupleImpl() = default;

    template <typename... Us>
        requires(sizeof...(Us) != 0)
             && (sizeof...(Us) == sizeof...(Ts))
             && (sizeof...(Us) != 1 || !std::same_as<std::decay_t<Us...[0]>, TupleImpl>)
             && (std::constructible_from<Ts, Us &&> && ...)
    explicit constexpr TupleImpl(Us&&... values)
        : Node<indices_T, Ts>{ std::forward<Us>(values) }...
    {
    }

    template <std::size_t index_T, typename Self_T>
            requires(index_T < sizeof...(Ts))
                 && std::derived_from<std::remove_cvref_t<Self_T>, TupleImpl>
        [[nodiscard]]
        friend constexpr auto get(Self_T&& self) noexcept
            -> forward_like_t<Ts... [index_T], Self_T>
        {
            return std::forward_like<Self_T>(
                self.internal::template Node<index_T, Ts...[index_T]>::value
            );
        }

        template <typename T, typename Self_T>
            requires contained_exactly_once_c<T, Ts...> && std::derived_from<std::remove_cvref_t<Self_T>, TupleImpl>
        [[nodiscard]]
        friend constexpr auto get(Self_T&& self) noexcept
            -> forward_like_t<T, Self_T>
    {
        constexpr static std::size_t index{ index_of_v<T, Ts...> };
        return std::forward_like<Self_T>(
            self.internal::template Node<index, Ts...[index]>::value
        );
    }
};

}   // namespace internal

export template <typename... Ts>
class Tuple : public internal::TupleImpl<std::make_index_sequence<sizeof...(Ts)>, Ts...> {
public:
    using internal::TupleImpl<std::make_index_sequence<sizeof...(Ts)>, Ts...>::TupleImpl;
};

}   // namespace kiln::util
