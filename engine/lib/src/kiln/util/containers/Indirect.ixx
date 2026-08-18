module;

#include <concepts>
#include <memory_resource>
#include <type_traits>
#include <utility>

#include "kiln/util/contract_macros.hpp"

export module kiln.util.containers.Indirect;

import kiln.util.concepts.decayed;
import kiln.util.contracts;
import kiln.util.ScopeFail;

namespace kiln::util {

export template <decayed_c T>
class Indirect {
public:
    using ValueType      = T;
    using allocator_type = std::pmr::polymorphic_allocator<T>;


    Indirect(const Indirect&)
        requires(std::copy_constructible<T>);
    Indirect(const Indirect&, const allocator_type& allocator)
        requires(std::copy_constructible<T>);
    Indirect(Indirect&&) noexcept;
    Indirect(Indirect&&, const allocator_type& allocator)
        requires(std::copy_constructible<T>);
    Indirect(Indirect&&, const allocator_type& allocator)
        requires(!std::copy_constructible<T>);
    ~Indirect();

    template <typename U>
        requires(
            !std::is_same_v<std::remove_cvref_t<U>, Indirect>
            && !std::is_same_v<std::remove_cvref_t<U>, std::in_place_t>
            && std::is_constructible_v<T, U &&>
        )
    explicit Indirect(U&& value);
    template <typename U>
        requires(
            !std::is_same_v<std::remove_cvref_t<U>, Indirect>
            && !std::is_same_v<std::remove_cvref_t<U>, std::in_place_t>
            && std::is_constructible_v<T, U &&>
        )
    explicit Indirect(std::allocator_arg_t, const allocator_type& allocator, U&& value);

    template <typename... Args_T>
        requires(std::is_constructible_v<T, Args_T && ...>)
    explicit Indirect(std::in_place_t, Args_T&&... args);
    template <typename... Args_T>
        requires(std::is_constructible_v<T, Args_T && ...>)
    explicit Indirect(
        std::allocator_arg_t,
        const allocator_type& allocator,
        std::in_place_t,
        Args_T&&... args
    );


    auto operator=(const Indirect&) -> Indirect&
        requires(std::copy_constructible<T>);
    // ReSharper disable once CppSpecialFunctionWithoutNoexceptSpecification
    // NOLINTNEXTLINE(*-noexcept-move-operations,*-noexcept-move,*-noexcept-move-constructor)
    auto operator=(Indirect&&) -> Indirect&
        requires(std::copy_constructible<T>);
    auto operator=(Indirect&&) noexcept -> Indirect&
        requires(!std::copy_constructible<T>);


    [[nodiscard]]
    auto operator->() noexcept -> T*;
    [[nodiscard]]
    auto operator->() const noexcept -> const T*;

    [[nodiscard]]
    auto operator*() noexcept -> T&;
    [[nodiscard]]
    auto operator*() const noexcept -> const T&;


    [[nodiscard]]
    auto get_allocator() const noexcept -> allocator_type;


    // ReSharper disable once CppSpecialFunctionWithoutNoexceptSpecification
    // NOLINTNEXTLINE(*-noexcept-swap)
    auto swap(Indirect&) -> void;

private:
    allocator_type m_allocator;
    T*             m_handle;


    auto release() -> void;
};

// ReSharper disable once CppSpecialFunctionWithoutNoexceptSpecification
// NOLINTNEXTLINE(*-noexcept-swap)
export template <typename T>
auto swap(Indirect<T>& lhs, Indirect<T>& rhs) -> void;

}   // namespace kiln::util

namespace kiln::util {

template <typename T>
[[nodiscard]]
auto copy_construct(std::pmr::polymorphic_allocator<T>& allocator, const T* other_handle)
    -> T*
{
    if (other_handle == nullptr)
    {
        return nullptr;
    }

    return allocator.template new_object<T>(*other_handle);
}

template <decayed_c T>
Indirect<T>::Indirect(const Indirect& other)
    requires(std::copy_constructible<T>)
    : m_allocator{ other.m_allocator },
      m_handle{ copy_construct(m_allocator, other.m_handle) }
{
}

template <decayed_c T>
Indirect<T>::Indirect(const Indirect& other, const allocator_type& allocator)
    requires(std::copy_constructible<T>)
    : m_allocator{ allocator },
      m_handle{ copy_construct(m_allocator, other.m_handle) }
{
}

template <decayed_c T>
Indirect<T>::Indirect(Indirect&& other) noexcept
    : m_allocator{ other.m_allocator },
      m_handle{ std::exchange(other.m_handle, nullptr) }
{
}

template <std::copyable T>
[[nodiscard]]
auto move_construct(
    std::pmr::polymorphic_allocator<T>&       allocator,
    const std::pmr::polymorphic_allocator<T>& other_allocator,
    T*&                                       other_handle
) -> T*
{
    if (allocator != other_allocator)
    {
        return copy_construct(allocator, other_handle);
    }

    return std::exchange(other_handle, nullptr);
}

template <decayed_c T>
Indirect<T>::Indirect(Indirect&& other, const allocator_type& allocator)
    requires(std::copy_constructible<T>)
    : m_allocator{ allocator },
      m_handle{ move_construct(m_allocator, other.m_allocator, other.m_handle) }
{
}

template <typename T>
[[nodiscard]]
auto assert_allocator_upon_move_only_move_construct(
    const std::pmr::polymorphic_allocator<T>&                  new_allocator,
    [[maybe_unused]] const std::pmr::polymorphic_allocator<T>& other_allocator
) -> std::pmr::polymorphic_allocator<T>
{
    PRECOND(new_allocator == other_allocator);
    return new_allocator;
}

template <decayed_c T>
Indirect<T>::Indirect(Indirect&& other, const allocator_type& allocator)
    requires(!std::copy_constructible<T>)
    : m_allocator{
          assert_allocator_upon_move_only_move_construct(allocator, other.m_allocator)
      },
      m_handle{ std::exchange(other.m_handle, nullptr) }
{
}

template <decayed_c T>
Indirect<T>::~Indirect()
{
    if (m_handle != nullptr)
    {
        m_allocator.delete_object(m_handle);
    }
}

template <decayed_c T>
template <typename U>
    requires(
        !std::is_same_v<std::remove_cvref_t<U>, Indirect<T>>
        && !std::is_same_v<std::remove_cvref_t<U>, std::in_place_t>
        && std::is_constructible_v<T, U &&>
    )
Indirect<T>::Indirect(U&& value)
    : Indirect{
          std::allocator_arg,
          std::pmr::get_default_resource(),
          std::forward<U>(value),
      }
{
}

template <decayed_c T>
template <typename U>
    requires(!std::is_same_v<std::remove_cvref_t<U>, Indirect<T>>
             && !std::is_same_v<std::remove_cvref_t<U>, std::in_place_t>
             && std::is_constructible_v<T, U &&>)
Indirect<T>::Indirect(std::allocator_arg_t, const allocator_type& allocator, U&& value)
    : m_allocator{ allocator },
      m_handle{ m_allocator.template new_object<T>(std::forward<U>(value)) }
{
}

template <decayed_c T>
template <typename... Args_T>
    requires(std::is_constructible_v<T, Args_T && ...>)
Indirect<T>::Indirect(const std::in_place_t in_place, Args_T&&... args)
    : Indirect{
          std::allocator_arg,
          std::pmr::get_default_resource(),
          in_place,
          std::forward<Args_T>(args)...,
      }
{
}

template <decayed_c T>
template <typename... Args_T>
    requires(std::is_constructible_v<T, Args_T && ...>)
Indirect<T>::Indirect(
    std::allocator_arg_t,
    const allocator_type& allocator,
    std::in_place_t,
    Args_T&&... args
)
    : m_allocator{ allocator },
      m_handle{ m_allocator.template new_object<T>(std::forward<Args_T>(args)...) }
{
}

template <decayed_c T>
auto Indirect<T>::operator=(const Indirect& other) -> Indirect&
    requires(std::copy_constructible<T>)
{
    if (this == &other)
    {
        return *this;
    }

    if (m_handle != nullptr && other.m_handle != nullptr)
    {
        *m_handle = *other.m_handle;
        return *this;
    }

    T* const        new_handle = other.m_handle == nullptr
                                   ? nullptr
                                   : m_allocator.template new_object<T>(*other.m_handle);
    const ScopeFail new_object_guard{
        [&] noexcept -> void
        {
            m_allocator.delete_object(new_handle);   //
        },
    };

    if (m_handle != nullptr)
    {
        m_allocator.delete_object(m_handle);
    }

    m_handle = new_handle;

    return *this;
}

template <decayed_c T>
// NOLINTNEXTLINE(*-noexcept-move-constructor)
auto Indirect<T>::operator=(Indirect&& other) -> Indirect&
    requires(std::copy_constructible<T>)
{
    if (this == *other)
    {
        return *this;
    }

    if (m_allocator != other.m_allocator)
    {
        operator=(other);
        return *this;
    }

    swap(other);
    other.release();

    return *this;
}

template <decayed_c T>
auto Indirect<T>::operator=(Indirect&& other) noexcept -> Indirect&
    requires(!std::copy_constructible<T>)
{
    if (this == &other)
    {
        return *this;
    }

    swap(other);
    other.release();

    return *this;
}

template <decayed_c T>
auto Indirect<T>::operator->() noexcept -> T*
{
    PRECOND(m_handle != nullptr);
    return m_handle;
}

template <decayed_c T>
auto Indirect<T>::operator->() const noexcept -> const T*
{
    PRECOND(m_handle != nullptr);
    return m_handle;
}

template <decayed_c T>
auto Indirect<T>::operator*() noexcept -> T&
{
    PRECOND(m_handle != nullptr);
    // ReSharper disable once CppDFANullDereference
    return *m_handle;
}

template <decayed_c T>
auto Indirect<T>::operator*() const noexcept -> const T&
{
    PRECOND(m_handle != nullptr);
    // ReSharper disable once CppDFANullDereference
    return *m_handle;
}

template <decayed_c T>
auto Indirect<T>::get_allocator() const noexcept -> allocator_type
{
    return m_allocator;
}

template <decayed_c T>
auto Indirect<T>::swap(Indirect& other) -> void
{
    PRECOND(m_allocator == other.m_allocator);

    std::swap(m_handle, other.m_handle);
}

template <decayed_c T>
auto Indirect<T>::release() -> void
{
    if (m_handle != nullptr)
    {
        m_allocator.delete_object(m_handle);
        m_handle = nullptr;
    }
}

// ReSharper disable once CppSpecialFunctionWithoutNoexceptSpecification
// NOLINTNEXTLINE(*-noexcept-swap)
template <typename T>
auto swap(Indirect<T>& lhs, Indirect<T>& rhs) -> void
{
    lhs.swap(rhs);
}

}   // namespace kiln::util
