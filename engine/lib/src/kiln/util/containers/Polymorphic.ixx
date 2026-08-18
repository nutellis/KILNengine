module;

#include <array>
#include <concepts>
#include <format>
#include <functional>
#include <memory_resource>
#include <type_traits>
#include <utility>
#include <variant>

#include "kiln/util/contract_macros.hpp"
#include "kiln/util/memory/no_unique_address.hpp"

export module kiln.util.containers.Polymorphic;

import kiln.util.concepts.decayed;
import kiln.util.concepts.nothrow_movable;
import kiln.util.concepts.specialization_of;
import kiln.util.concepts.strips_to;
import kiln.util.concepts.storable;
import kiln.util.contracts;
import kiln.util.reflection;
import kiln.util.ScopeFail;
import kiln.util.type_traits.forward_like;

namespace kiln::util {

export consteval auto default_polymorphic_size() -> std::size_t
{
    return 3 * sizeof(void*);
}

export consteval auto default_polymorphic_alignment() -> std::size_t
{
    return alignof(std::max_align_t);
}

template <typename T>
concept interface_c = std::destructible<T>;

export template <
    interface_c Interface_T,
    bool        is_move_only_T = false,
    std::size_t size_T         = default_polymorphic_size(),
    std::size_t alignment_T    = default_polymorphic_alignment()>
class Polymorphic;

template <typename Interface_T, bool is_move_only_T, std::size_t size_T, std::size_t alignment_T>
class EraseMechanism;

template <typename Interface_T, bool is_move_only_T, std::size_t size_T, std::size_t alignment_T>
    requires(size_T != 0)
class EraseMechanism<Interface_T, is_move_only_T, size_T, alignment_T> {
public:
    union Storage {
        Interface_T* handle;
        alignas(alignment_T) std::array<std::byte, size_T> small_buffer;
    };

    template <typename T>
    constexpr explicit EraseMechanism(std::in_place_type_t<T>) noexcept;

    [[nodiscard]]
    auto copy_construct(
        std::pmr::polymorphic_allocator<>& allocator,
        const Storage&                     source_storage
    ) const -> Storage
        requires(!is_move_only_T);
    [[nodiscard]]
    constexpr auto move_construct(Storage&& source_storage) const noexcept -> Storage;
    [[nodiscard]]
    auto move_construct(
        std::pmr::polymorphic_allocator<>&       destination_allocator,
        const std::pmr::polymorphic_allocator<>& source_allocator,
        Storage&&                                source_storage
    ) const -> Storage;
    constexpr auto move_construct_at(
        Storage&  destination_storage,
        Storage&& source_storage
    ) const noexcept -> void;
    auto move_construct_at(
        std::pmr::polymorphic_allocator<>&       destination_allocator,
        Storage&                                 destination_storage,
        const std::pmr::polymorphic_allocator<>& source_allocator,
        Storage&&                                source_storage
    ) const -> void;
    auto drop(std::pmr::polymorphic_allocator<>& allocator, Storage& storage) const
        -> void;

    template <typename T, typename... Args_T>
    [[nodiscard]]
    static auto construct(
        std::allocator_arg_t,
        std::pmr::polymorphic_allocator<>& allocator,
        std::in_place_type_t<T>,
        Args_T&&... args
    ) -> Storage;

    auto copy_assign(
        std::pmr::polymorphic_allocator<>& destination_allocator,
        Storage&                           destination_storage,
        const EraseMechanism&              destination_erase_mechanism,
        const Storage&                     source_storage
    ) const -> void
        requires(!is_move_only_T);

    [[nodiscard]]
    constexpr auto address_of(Storage& storage) const -> Interface_T*;
    [[nodiscard]]
    constexpr auto address_of(const Storage& storage) const -> const Interface_T*;
    [[nodiscard]]
    constexpr auto dereference(Storage& storage) const -> Interface_T&;
    [[nodiscard]]
    constexpr auto dereference(const Storage& storage) const -> const Interface_T&;

    [[nodiscard]]
    auto type_hash() const -> uint64_t;
    [[nodiscard]]
    auto type_name() const -> std::string_view;
    [[nodiscard]]
    auto uses_small_buffer() const noexcept -> bool;

    constexpr auto swap(
        std::pmr::polymorphic_allocator<>& lhs_allocator,
        Storage&                           lhs_storage,
        std::pmr::polymorphic_allocator<>& rhs_allocator,
        Storage&                           rhs_storage,
        const EraseMechanism&              rhs_erase_mechanism
    ) const -> void;
    auto reset(std::pmr::polymorphic_allocator<>& allocator, Storage& storage) const
        -> void;

private:
    struct VTable;

    std::reference_wrapper<const VTable> m_vtable;
};

template <typename Interface_T, bool is_move_only_T, std::size_t alignment_T>
class EraseMechanism<Interface_T, is_move_only_T, 0, alignment_T> {
public:
    struct Storage {
        Interface_T* handle;
    };

    template <typename T>
    constexpr explicit EraseMechanism(std::in_place_type_t<T>) noexcept
        requires(!is_move_only_T);
    template <typename T>
    constexpr explicit EraseMechanism(std::in_place_type_t<T>) noexcept
        requires(is_move_only_T);

    [[nodiscard]]
    auto copy_construct(
        std::pmr::polymorphic_allocator<>& allocator,
        const Storage&                     source_storage
    ) const -> Storage
        requires(!is_move_only_T);
    [[nodiscard]]
    constexpr static auto move_construct(Storage&& source_storage) noexcept -> Storage;
    [[nodiscard]]
    auto move_construct(
        std::pmr::polymorphic_allocator<>&       destination_allocator,
        const std::pmr::polymorphic_allocator<>& source_allocator,
        Storage&&                                source_storage
    ) -> Storage
        requires(!is_move_only_T);
    [[nodiscard]]
    static auto move_construct(
        const std::pmr::polymorphic_allocator<>& destination_allocator,
        const std::pmr::polymorphic_allocator<>& source_allocator,
        Storage&&                                source_storage
    ) noexcept -> Storage
        requires is_move_only_T;
    static auto drop(std::pmr::polymorphic_allocator<>& allocator, Storage& storage)
        -> void;

    template <typename T, typename... Args_T>
    [[nodiscard]]
    static auto construct(
        std::allocator_arg_t,
        std::pmr::polymorphic_allocator<>& allocator,
        std::in_place_type_t<T>,
        Args_T&&... args
    ) -> Storage;

    auto copy_assign(
        std::pmr::polymorphic_allocator<>& destination_allocator,
        Storage&                           destination_storage,
        const EraseMechanism&              destination_erase_mechanism,
        const Storage&                     source_storage
    ) const -> void
        requires(!is_move_only_T);

    [[nodiscard]]
    constexpr static auto address_of(Storage& storage) -> Interface_T*;
    [[nodiscard]]
    constexpr static auto address_of(const Storage& storage) -> const Interface_T*;
    [[nodiscard]]
    constexpr static auto dereference(Storage& storage) -> Interface_T&;
    [[nodiscard]]
    constexpr static auto dereference(const Storage& storage) -> const Interface_T&;

    [[nodiscard]]
    auto type_hash() const -> uint64_t;
    [[nodiscard]]
    auto type_name() const -> std::string_view;

    constexpr static auto swap(
        const std::pmr::polymorphic_allocator<>& lhs_allocator,
        Storage&                                 lhs_storage,
        const std::pmr::polymorphic_allocator<>& rhs_allocator,
        Storage&                                 rhs_storage,
        const EraseMechanism&                    rhs_erase_mechanism
    ) -> void;
    static auto reset(std::pmr::polymorphic_allocator<>& allocator, Storage& storage)
        -> void;

private:
    struct VTable;

#ifndef KILN_DEBUG
    struct EmptyVTable {};
#else
    struct DiagnosticVTable;
    using EmptyVTable = std::reference_wrapper<const DiagnosticVTable>;
#endif

    [[kiln_no_unique_address]]
    std::conditional_t<!is_move_only_T, std::reference_wrapper<const VTable>, EmptyVTable>
        m_vtable;
};

namespace internal {

class PolymorphicBase {};

}   // namespace internal

export template <typename T, typename Polymorphic_T>
concept storable_in_polymorphic_c
    = std::derived_from<Polymorphic_T, internal::PolymorphicBase>
   && std::derived_from<T, typename Polymorphic_T::InterfaceType>
   && ::kiln::util::storable_c<T>
   && (Polymorphic_T::is_move_only() || std::copyable<T>);

export template <typename T, typename Polymorphic_T>
concept decays_to_storable_in_polymorphic_c
    = storable_in_polymorphic_c<std::decay_t<T>, Polymorphic_T>;

export template <decayed_c T, typename Polymorphic_T>
    requires storable_in_polymorphic_c<T, std::remove_cvref_t<Polymorphic_T>>
[[nodiscard]]
auto any_cast(Polymorphic_T&& polymorphic) -> forward_like_t<T, Polymorphic_T>;

export template <decayed_c T, typename Polymorphic_T>
    requires std::derived_from<std::remove_cvref_t<Polymorphic_T>, internal::PolymorphicBase>
[[nodiscard]]
auto reinterpret_any_cast(Polymorphic_T&& polymorphic)
    -> forward_like_t<T, Polymorphic_T>;

export template <
    interface_c Interface_T,
    bool        is_move_only_T,
    std::size_t size_T,
    std::size_t alignment_T>
class Polymorphic : public internal::PolymorphicBase {
public:
    using InterfaceType  = Interface_T;
    using allocator_type = std::pmr::polymorphic_allocator<>;


    [[nodiscard]]
    consteval static auto is_move_only() -> bool;
    [[nodiscard]]
    consteval static auto size() -> std::size_t;
    [[nodiscard]]
    consteval static auto alignment() -> std::size_t;


    Polymorphic(const Polymorphic&)
        requires(!is_move_only_T);
    Polymorphic(const Polymorphic&, const allocator_type& allocator)
        requires(!is_move_only_T);
    Polymorphic(Polymorphic&&) noexcept;
    Polymorphic(Polymorphic&&, const allocator_type& allocator);
    ~Polymorphic();

    template <typename T>
    explicit Polymorphic(T&& value)
        requires(!strips_to_c<T, Polymorphic>)
             && (!util::specialization_of_c<std::remove_cvref_t<T>, std::in_place_type_t>)
             && std::constructible_from<std::remove_cvref_t<T>, T&&>
             && storable_in_polymorphic_c<std::remove_cvref_t<T>, Polymorphic>;
    template <typename T>
    explicit Polymorphic(std::allocator_arg_t, const allocator_type& allocator, T&& value)
        requires(!strips_to_c<T, Polymorphic>)
             && (!util::specialization_of_c<std::remove_cvref_t<T>, std::in_place_type_t>)
             && std::constructible_from<std::remove_cvref_t<T>, T&&>
             && storable_in_polymorphic_c<std::remove_cvref_t<T>, Polymorphic>;

    template <typename T, typename... Args_T>
    explicit Polymorphic(std::in_place_type_t<T>, Args_T&&... args)
        requires std::constructible_from<T, Args_T&&...>
              && storable_in_polymorphic_c<T, Polymorphic>;
    template <typename T, typename... Args_T>
    explicit Polymorphic(
        std::allocator_arg_t,
        const allocator_type& allocator,
        std::in_place_type_t<T>,
        Args_T&&... args
    )
        requires std::constructible_from<T, Args_T&&...>
              && storable_in_polymorphic_c<T, Polymorphic>;

    auto operator=(const Polymorphic&) -> Polymorphic&
        requires(!is_move_only_T);
    // ReSharper disable once CppSpecialFunctionWithoutNoexceptSpecification
    // NOLINTNEXTLINE(*-noexcept-move-operations,*-noexcept-move,*-noexcept-move-constructor)
    auto operator=(Polymorphic&&) -> Polymorphic&
        requires(!is_move_only_T);
    auto operator=(Polymorphic&&) noexcept -> Polymorphic&
        requires is_move_only_T;


    [[nodiscard]]
    auto operator->() noexcept -> Interface_T*;
    [[nodiscard]]
    auto operator->() const noexcept -> const Interface_T*;

    [[nodiscard]]
    auto operator*() noexcept -> Interface_T&;
    [[nodiscard]]
    auto operator*() const noexcept -> const Interface_T&;


    [[nodiscard]]
    auto get_allocator() const noexcept -> allocator_type;

    // ReSharper disable once CppSpecialFunctionWithoutNoexceptSpecification
    // NOLINTNEXTLINE(*-noexcept-swap)
    auto swap(Polymorphic& other) -> void;


    template <decayed_c T, typename Polymorphic_T>
        requires storable_in_polymorphic_c<T, std::remove_cvref_t<Polymorphic_T>>
    friend auto any_cast(Polymorphic_T&& polymorphic) -> forward_like_t<T, Polymorphic_T>;

    template <decayed_c T, typename Polymorphic_T>
        requires std::derived_from<std::remove_cvref_t<Polymorphic_T>, PolymorphicBase>
    friend auto reinterpret_any_cast(Polymorphic_T&& polymorphic)
        -> forward_like_t<T, Polymorphic_T>;

private:
    allocator_type                                                            m_allocator;
    EraseMechanism<Interface_T, is_move_only_T, size_T, alignment_T>::Storage m_storage;
    [[kiln_no_unique_address]]
    EraseMechanism<Interface_T, is_move_only_T, size_T, alignment_T> m_erase_mechanism;


    auto reset() -> void;
};

// ReSharper disable once CppSpecialFunctionWithoutNoexceptSpecification
// NOLINTNEXTLINE(*-noexcept-swap)
export template <
    typename Interface_T,
    bool        is_move_only_T,
    std::size_t size_T,
    std::size_t alignment_T>
auto swap(
    Polymorphic<Interface_T, is_move_only_T, size_T, alignment_T>& lhs,
    Polymorphic<Interface_T, is_move_only_T, size_T, alignment_T>& rhs
) -> void;

}   // namespace kiln::util

namespace kiln::util {

template <typename Interface_T, bool is_move_only_T, std::size_t size_T, std::size_t alignment_T>
    requires(size_T != 0)
struct EraseMechanism<Interface_T, is_move_only_T, size_T, alignment_T>::VTable {
    using CopyConstructAtFunc = auto(
        std::pmr::polymorphic_allocator<>& allocator,
        Storage&                           destination_storage,
        const Storage&                     source_storage
    ) -> void;
    using MoveConstructAtFunc
        = auto(Storage& destination_storage, Storage&& source_storage) -> void;
    using MoveConstructAtUsingAllocatorFunc = auto(
        std::pmr::polymorphic_allocator<>&       destination_allocator,
        Storage&                                 destination_storage,
        const std::pmr::polymorphic_allocator<>& source_allocator,
        Storage&&                                source_storage
    ) -> void;
    using DropFunc = auto(std::pmr::polymorphic_allocator<>& allocator, Storage& storage)
        -> void;
    using CopyAssignFunc = auto(
        std::pmr::polymorphic_allocator<>& allocator,
        Storage&                           destination_storage,
        const EraseMechanism&              destination_erase_mechanism,
        const Storage&                     source_storage
    ) -> void;
    using AddressOfFunc        = auto(Storage&) -> Interface_T*;
    using ConstAddressOfFunc   = auto(const Storage&) -> const Interface_T*;
    using DereferenceFunc      = auto(Storage&) -> Interface_T&;
    using ConstDereferenceFunc = auto(const Storage&) -> const Interface_T&;
    using TypeHashFunc         = auto() -> uint64_t;
    using TypeNameFunc         = auto() -> std::string_view;
    using UsesSmallBufferFunc  = auto() -> bool;
    using SwapFunc             = auto(
        std::pmr::polymorphic_allocator<>& lhs_allocator,
        Storage&                           lhs_storage,
        std::pmr::polymorphic_allocator<>& rhs_allocator,
        Storage&                           rhs_storage,
        const EraseMechanism&              rhs_erase_mechanism
    ) -> void;
    using ReleaseFunc
        = auto(std::pmr::polymorphic_allocator<>& allocator, Storage& storage) -> void;

    template <typename T>
    struct Operations;


    std::add_pointer_t<CopyConstructAtFunc>     copy_construct_at;
    std::reference_wrapper<MoveConstructAtFunc> move_construct_at;
    std::reference_wrapper<MoveConstructAtUsingAllocatorFunc>
                                                 move_construct_at_using_allocator;
    std::reference_wrapper<DropFunc>             drop;
    std::add_pointer_t<CopyAssignFunc>           copy_assign;
    std::reference_wrapper<AddressOfFunc>        address_of;
    std::reference_wrapper<ConstAddressOfFunc>   const_address_of;
    std::reference_wrapper<DereferenceFunc>      dereference;
    std::reference_wrapper<ConstDereferenceFunc> const_dereference;
    std::reference_wrapper<TypeHashFunc>         type_hash;
    std::reference_wrapper<TypeNameFunc>         type_name;
    std::reference_wrapper<UsesSmallBufferFunc>  uses_small_buffer;
    std::reference_wrapper<SwapFunc>             swap;
    std::reference_wrapper<ReleaseFunc>          reset;
};

template <typename Interface_T, bool is_move_only_T, std::size_t size_T, std::size_t alignment_T>
    requires(size_T != 0)
template <typename T>
struct EraseMechanism<Interface_T, is_move_only_T, size_T, alignment_T>::VTable::Operations {
    constexpr static auto uses_small_buffer() noexcept -> bool
    {
        return sizeof(T) <= size_T
            && alignment_T % alignof(T) == 0
            && nothrow_movable_c<T>;
    }

    template <typename... Args_T>
    static auto create_at(
        std::pmr::polymorphic_allocator<>& allocator,
        Storage&                           storage,
        Args_T&&... args
    ) -> void
    {
        if constexpr (uses_small_buffer())
        {
            allocator.construct(
                static_cast<T*>(storage.small_buffer.data()),
                std::forward<Args_T>(args)...
            );
        }
        else
        {
            storage.handle = allocator.new_object<T>(std::forward<Args_T>(args)...);
        }
    }

    static auto copy_construct_at(
        std::pmr::polymorphic_allocator<>& allocator,
        Storage&                           destination_storage,
        const Storage&                     source_storage
    ) -> void
    {
        if constexpr (uses_small_buffer())
        {
            create_at(
                allocator,
                destination_storage,
                *static_cast<const T*>(source_storage.small_buffer.data())
            );
        }
        else
        {
            if (source_storage.handle == nullptr)
            {
                destination_storage.handle = nullptr;
                return;
            }

            create_at(
                allocator,
                destination_storage,
                static_cast<const T&>(*source_storage.handle)
            );
        }
    }

    constexpr static auto move_construct_at(
        Storage&  destination_storage,
        Storage&& source_storage
    ) -> void
    {
        if constexpr (uses_small_buffer())
        {
            std::construct_at(
                static_cast<T*>(destination_storage.small_buffer.data()),
                std::move(*static_cast<T*>(source_storage.small_buffer.data()))
            );
        }
        else
        {
            destination_storage.handle = std::exchange(source_storage.handle, nullptr);
        }
    }

    constexpr static auto move_construct_at_using_allocator(
        std::pmr::polymorphic_allocator<>&       destination_allocator,
        Storage&                                 destination_storage,
        const std::pmr::polymorphic_allocator<>& source_allocator,
        Storage&&                                source_storage
    ) -> void
    {
        if constexpr (uses_small_buffer())
        {
            destination_allocator.construct(
                static_cast<T*>(destination_storage.small_buffer.data()),
                std::move(*static_cast<T*>(source_storage.small_buffer.data()))
            );
        }
        else
        {
            if constexpr (is_move_only_T)
            {
                PRECOND(destination_allocator == source_allocator);
                destination_storage.handle
                    = std::exchange(source_storage.handle, nullptr);
            }
            else
            {
                if (destination_allocator == source_allocator)
                {
                    destination_storage.handle
                        = std::exchange(source_storage.handle, nullptr);
                }
                else
                {
                    copy_construct_at(
                        destination_allocator,
                        destination_storage,
                        source_storage
                    );
                }
            }
        }
    }

    constexpr static auto drop(
        std::pmr::polymorphic_allocator<>& allocator,
        Storage&                           storage
    )
    {
        if constexpr (uses_small_buffer())
        {
            std::destroy_at(static_cast<T*>(storage.small_buffer.data()));
        }
        else
        {
            if (storage.handle != nullptr)
            {
                allocator.delete_object(static_cast<T*>(storage.handle));
            }
        }
    }

    static auto copy_assign(
        std::pmr::polymorphic_allocator<>& destination_allocator,
        Storage&                           destination_storage,
        const EraseMechanism&              destination_erase_mechanism,
        const Storage&                     source_storage
    ) -> void
    {
        if constexpr (uses_small_buffer())
        {
            if (destination_erase_mechanism.type_hash() == type_hash())
            {
                *static_cast<T*>(destination_storage.small_buffer.data())
                    = *static_cast<T*>(source_storage.small_buffer.data());
                return;
            }

            T new_object{ *static_cast<T*>(source_storage.small_buffer.data()) };

            destination_erase_mechanism.drop(destination_allocator, destination_storage);

            destination_allocator.construct(
                static_cast<T*>(destination_storage.small_buffer.data()),
                new_object
            );
        }
        else
        {
            if (destination_erase_mechanism.type_hash() == type_hash()
                && destination_storage.handle != nullptr
                && source_storage.handle != nullptr)
            {
                static_cast<T&>(*destination_storage.handle)
                    = static_cast<const T&>(*source_storage.handle);
                return;
            }

            Interface_T* const new_object
                = source_storage.handle == nullptr
                    ? nullptr
                    : destination_allocator.new_object<T>(
                          static_cast<const T&>(*source_storage.handle)
                      );
            const ScopeFail new_object_guard{
                [&] noexcept -> void
                {
                    if (new_object != nullptr)
                    {
                        destination_allocator.delete_object(new_object);
                    }
                },
            };

            destination_erase_mechanism.drop(destination_allocator, destination_storage);

            destination_storage.handle = new_object;
        }
    }

    constexpr static auto address_of(Storage& storage) -> Interface_T*
    {
        if constexpr (uses_small_buffer())
        {
            return static_cast<Interface_T*>(storage.small_buffer.data());
        }
        else
        {
            return storage.handle;
        }
    }

    constexpr static auto const_address_of(const Storage& storage) -> const Interface_T*
    {
        if constexpr (uses_small_buffer())
        {
            return static_cast<const Interface_T*>(storage.small_buffer.data());
        }
        else
        {
            return storage.handle;
        }
    }

    constexpr static auto dereference(Storage& storage) -> Interface_T&
    {
        if constexpr (uses_small_buffer())
        {
            return *static_cast<Interface_T*>(storage.small_buffer.data());
        }
        else
        {
            PRECOND(storage.handle != nullptr);
            return *storage.handle;
        }
    }

    constexpr static auto const_dereference(const Storage& storage) -> const Interface_T&
    {
        if constexpr (uses_small_buffer())
        {
            return *static_cast<const Interface_T*>(storage.small_buffer.data());
        }
        else
        {
            PRECOND(storage.handle != nullptr);
            return *storage.handle;
        }
    }

    [[nodiscard]]
    constexpr static auto type_hash() -> uint64_t
    {
        return util::hash_u64<T>();
    }

    [[nodiscard]]
    constexpr static auto type_name() -> std::string_view
    {
        return util::name_of<T>();
    }

    constexpr static auto swap(
        std::pmr::polymorphic_allocator<>& lhs_allocator,
        Storage&                           lhs_storage,
        std::pmr::polymorphic_allocator<>& rhs_allocator,
        Storage&                           rhs_storage,
        const EraseMechanism&              rhs_erase_mechanism
    ) -> void
    {
        if constexpr (uses_small_buffer())
        {
            if (type_hash() == rhs_erase_mechanism.type_hash())
            {
                std::swap(
                    *static_cast<T*>(lhs_storage.small_buffer.data()),
                    *static_cast<T*>(rhs_storage.small_buffer.data())
                );
            }
            else
            {
                PRECOND(
                    rhs_erase_mechanism.uses_small_buffer()
                    || lhs_allocator == rhs_allocator
                );
                Storage tmp;
                rhs_erase_mechanism.move_construct_at(
                    lhs_allocator,
                    tmp,
                    rhs_allocator,
                    std::move(rhs_storage)
                );
                rhs_erase_mechanism.drop(rhs_allocator, rhs_storage);
                const ScopeFail tmp_guard{
                    [&] noexcept -> void { rhs_erase_mechanism.drop(lhs_allocator, tmp); }
                };

                std::construct_at(
                    static_cast<T*>(rhs_storage.small_buffer.data()),
                    std::move(*static_cast<T*>(lhs_storage.small_buffer.data()))
                );

                rhs_erase_mechanism.move_construct_at(lhs_storage, std::move(tmp));
                rhs_erase_mechanism.drop(lhs_allocator, tmp);
            }
        }
        else
        {
            PRECOND(lhs_allocator == rhs_allocator);
            if (type_hash() == rhs_erase_mechanism.type_hash())
            {
                std::swap(lhs_storage.handle, rhs_storage.handle);
            }
            else
            {
                Interface_T* tmp{ lhs_storage.handle };

                rhs_erase_mechanism.move_construct_at(
                    lhs_allocator,
                    lhs_storage,
                    rhs_allocator,
                    std::move(rhs_storage)
                );
                rhs_erase_mechanism.drop(rhs_allocator, rhs_storage);

                rhs_storage.handle = tmp;
            }
        }
    }

    constexpr static auto reset(
        std::pmr::polymorphic_allocator<>& allocator,
        Storage&                           storage
    ) -> void
    {
        /*
         * Small buffer reset is no-op. It will destroy at drop.
         */

        if constexpr (!uses_small_buffer())
        {
            if (storage.handle != nullptr)
            {
                allocator.delete_object(static_cast<T*>(storage.handle));
                storage.handle = nullptr;
            }
        }
    }

    constexpr static VTable vtable{
        .copy_construct_at = [] -> auto
        {
            if constexpr (is_move_only_T)
            {
                return nullptr;
            }
            else
            {
                return copy_construct_at;
            }
        }(),
        .move_construct_at                 = move_construct_at,
        .move_construct_at_using_allocator = move_construct_at_using_allocator,
        .drop                              = drop,
        .copy_assign                       = [] -> auto
        {
            if constexpr (is_move_only_T)
            {
                return nullptr;
            }
            else
            {
                return copy_assign;
            }
        }(),
        .address_of        = address_of,
        .const_address_of  = const_address_of,
        .dereference       = dereference,
        .const_dereference = const_dereference,
        .type_hash         = type_hash,
        .type_name         = type_name,
        .uses_small_buffer = uses_small_buffer,
        .swap              = swap,
        .reset             = reset,
    };
};

template <typename Interface_T, bool is_move_only_T, std::size_t alignment_T>
struct EraseMechanism<Interface_T, is_move_only_T, 0, alignment_T>::VTable {
    using CopyConstructFunc
        = auto(std::pmr::polymorphic_allocator<>& allocator, const Storage& source_storage)
            -> Storage;
    using CopyAssignFunc = auto(
        std::pmr::polymorphic_allocator<>& allocator,
        Storage&                           destination_storage,
        const EraseMechanism&              destination_erase_mechanism,
        const Storage&                     source_storage
    ) -> void;
    using TypeHashFunc = auto() -> uint64_t;
    using TypeNameFunc = auto() -> std::string_view;

    template <typename T>
    struct Operations;


    std::reference_wrapper<CopyConstructFunc> copy_construct;
    std::reference_wrapper<CopyAssignFunc>    copy_assign;
    std::reference_wrapper<TypeHashFunc>      type_hash;
    std::reference_wrapper<TypeNameFunc>      type_name;
};

#ifdef KILN_DEBUG
template <typename Interface_T, bool is_move_only_T, std::size_t alignment_T>
struct EraseMechanism<Interface_T, is_move_only_T, 0, alignment_T>::DiagnosticVTable {
    using TypeHashFunc = auto() -> uint64_t;
    using TypeNameFunc = auto() -> std::string_view;

    template <typename T>
    struct Operations;


    std::reference_wrapper<TypeHashFunc> type_hash;
    std::reference_wrapper<TypeNameFunc> type_name;
};
#endif

template <typename Interface_T, bool is_move_only_T, std::size_t alignment_T>
template <typename T>
struct EraseMechanism<Interface_T, is_move_only_T, 0, alignment_T>::VTable::Operations {
    static auto copy_construct(
        std::pmr::polymorphic_allocator<>& allocator,
        const Storage&                     source_storage
    ) -> Storage
    {
        Storage result;


        if (source_storage.handle == nullptr)
        {
            result.handle = nullptr;
            return result;
        }

        result.handle
            = allocator.new_object<T>(static_cast<const T&>(*source_storage.handle));
        return result;
    }

    static auto copy_assign(
        std::pmr::polymorphic_allocator<>& destination_allocator,
        Storage&                           destination_storage,
        const EraseMechanism&              destination_erase_mechanism,
        const Storage&                     source_storage
    ) -> void
    {
        if (destination_erase_mechanism.type_hash() == type_hash()
            && destination_storage.handle != nullptr
            && source_storage.handle != nullptr)
        {
            static_cast<T&>(*destination_storage.handle)
                = static_cast<const T&>(*source_storage.handle);
            return;
        }

        Interface_T* const new_object
            = source_storage.handle == nullptr
                ? nullptr
                : destination_allocator.new_object<T>(
                      static_cast<const T&>(*source_storage.handle)
                  );
        const ScopeFail new_object_guard{
            [&] noexcept -> void
            {
                destination_allocator.delete_object(new_object);   //
            },
        };

        if (destination_storage.handle != nullptr)
        {
            destination_allocator.delete_object(destination_storage.handle);
        }

        destination_storage.handle = new_object;
    }

    [[nodiscard]]
    constexpr static auto type_hash() -> uint64_t
    {
        return util::hash_u64<T>();
    }

    [[nodiscard]]
    constexpr static auto type_name() -> std::string_view
    {
        return util::name_of<T>();
    }

    constexpr static VTable vtable{
        .copy_construct = copy_construct,
        .copy_assign    = copy_assign,
        .type_hash      = type_hash,
        .type_name      = type_name,
    };
};

#ifdef KILN_DEBUG
template <typename Interface_T, bool is_move_only_T, std::size_t alignment_T>
template <typename T>
struct EraseMechanism<Interface_T, is_move_only_T, 0, alignment_T>::DiagnosticVTable::
    Operations   //
{
    [[nodiscard]]
    constexpr static auto type_hash() -> uint64_t
    {
        return util::hash_u64<T>();
    }

    [[nodiscard]]
    constexpr static auto type_name() -> std::string_view
    {
        return util::name_of<T>();
    }

    constexpr static DiagnosticVTable vtable{
        .type_hash = type_hash,
        .type_name = type_name,
    };
};
#endif

template <typename Interface_T, bool is_move_only_T, std::size_t size_T, std::size_t alignment_T>
    requires(size_T != 0)
template <typename T>
constexpr EraseMechanism<Interface_T, is_move_only_T, size_T, alignment_T>::EraseMechanism(
    std::in_place_type_t<T>
) noexcept
    : m_vtable{ VTable::template Operations<T>::vtable }
{
}

template <typename Interface_T, bool is_move_only_T, std::size_t size_T, std::size_t alignment_T>
    requires(size_T != 0)
auto EraseMechanism<Interface_T, is_move_only_T, size_T, alignment_T>::copy_construct(
    std::pmr::polymorphic_allocator<>& allocator,
    const Storage&                     source_storage
) const -> Storage
    requires(!is_move_only_T)
{
    Storage result;
    m_vtable.get().copy_construct_at(allocator, result, source_storage);
    return result;
}

template <typename Interface_T, bool is_move_only_T, std::size_t size_T, std::size_t alignment_T>
    requires(size_T != 0)
constexpr auto
    EraseMechanism<Interface_T, is_move_only_T, size_T, alignment_T>::move_construct(
        Storage&& source_storage
    ) const noexcept -> Storage
{
    Storage result;
    move_construct_at(result, std::move(source_storage));
    return result;
}

template <typename Interface_T, bool is_move_only_T, std::size_t size_T, std::size_t alignment_T>
    requires(size_T != 0)
auto EraseMechanism<Interface_T, is_move_only_T, size_T, alignment_T>::move_construct(
    std::pmr::polymorphic_allocator<>&       destination_allocator,
    const std::pmr::polymorphic_allocator<>& source_allocator,
    Storage&&                                source_storage
) const -> Storage
{
    Storage result;
    move_construct_at(
        destination_allocator,
        result,
        source_allocator,
        std::move(source_storage)
    );
    return result;
}

template <typename Interface_T, bool is_move_only_T, std::size_t size_T, std::size_t alignment_T>
    requires(size_T != 0)
constexpr auto
    EraseMechanism<Interface_T, is_move_only_T, size_T, alignment_T>::move_construct_at(
        Storage&  destination_storage,
        Storage&& source_storage
    ) const noexcept -> void
{
    return m_vtable.get()
        .move_construct_at(destination_storage, std::move(source_storage));
}

template <typename Interface_T, bool is_move_only_T, std::size_t size_T, std::size_t alignment_T>
    requires(size_T != 0)
auto EraseMechanism<Interface_T, is_move_only_T, size_T, alignment_T>::move_construct_at(
    std::pmr::polymorphic_allocator<>&       destination_allocator,
    Storage&                                 destination_storage,
    const std::pmr::polymorphic_allocator<>& source_allocator,
    Storage&&                                source_storage
) const -> void
{
    m_vtable.get().move_construct_at_using_allocator(
        destination_allocator,
        destination_storage,
        source_allocator,
        std::move(source_storage)
    );
}

template <typename Interface_T, bool is_move_only_T, std::size_t size_T, std::size_t alignment_T>
    requires(size_T != 0)
auto EraseMechanism<Interface_T, is_move_only_T, size_T, alignment_T>::drop(
    std::pmr::polymorphic_allocator<>& allocator,
    Storage&                           storage
) const -> void
{
    m_vtable.get().drop(allocator, storage);
}

template <typename Interface_T, bool is_move_only_T, std::size_t size_T, std::size_t alignment_T>
    requires(size_T != 0)
template <typename T, typename... Args_T>
auto EraseMechanism<Interface_T, is_move_only_T, size_T, alignment_T>::construct(
    std::allocator_arg_t,
    std::pmr::polymorphic_allocator<>& allocator,
    std::in_place_type_t<T>,
    Args_T&&... args
) -> Storage
{
    Storage result;
    VTable::template Operations<T>::create_at(
        allocator,
        result,
        std::forward<Args_T>(args)...
    );
    return result;
}

template <typename Interface_T, bool is_move_only_T, std::size_t size_T, std::size_t alignment_T>
    requires(size_T != 0)
auto EraseMechanism<Interface_T, is_move_only_T, size_T, alignment_T>::copy_assign(
    std::pmr::polymorphic_allocator<>& destination_allocator,
    Storage&                           destination_storage,
    const EraseMechanism&              destination_erase_mechanism,
    const Storage&                     source_storage
) const -> void
    requires(!is_move_only_T)
{
    return m_vtable.get().copy_assign(
        destination_allocator,
        destination_storage,
        destination_erase_mechanism,
        source_storage
    );
}

template <typename Interface_T, bool is_move_only_T, std::size_t size_T, std::size_t alignment_T>
    requires(size_T != 0)
constexpr auto
    EraseMechanism<Interface_T, is_move_only_T, size_T, alignment_T>::address_of(
        Storage& storage
    ) const -> Interface_T*
{
    return m_vtable.get().address_of(storage);
}

template <typename Interface_T, bool is_move_only_T, std::size_t size_T, std::size_t alignment_T>
    requires(size_T != 0)
constexpr auto
    EraseMechanism<Interface_T, is_move_only_T, size_T, alignment_T>::address_of(
        const Storage& storage
    ) const -> const Interface_T*
{
    return m_vtable.get().const_address_of(storage);
}

template <typename Interface_T, bool is_move_only_T, std::size_t size_T, std::size_t alignment_T>
    requires(size_T != 0)
constexpr auto
    EraseMechanism<Interface_T, is_move_only_T, size_T, alignment_T>::dereference(
        Storage& storage
    ) const -> Interface_T&
{
    return m_vtable.get().dereference(storage);
}

template <typename Interface_T, bool is_move_only_T, std::size_t size_T, std::size_t alignment_T>
    requires(size_T != 0)
constexpr auto
    EraseMechanism<Interface_T, is_move_only_T, size_T, alignment_T>::dereference(
        const Storage& storage
    ) const -> const Interface_T&
{
    return m_vtable.get().const_dereference(storage);
}

template <typename Interface_T, bool is_move_only_T, std::size_t size_T, std::size_t alignment_T>
    requires(size_T != 0)
auto EraseMechanism<Interface_T, is_move_only_T, size_T, alignment_T>::type_hash() const
    -> uint64_t
{
    return m_vtable.get().type_hash();
}

template <typename Interface_T, bool is_move_only_T, std::size_t size_T, std::size_t alignment_T>
    requires(size_T != 0)
auto EraseMechanism<Interface_T, is_move_only_T, size_T, alignment_T>::type_name() const
    -> std::string_view
{
    return m_vtable.get().type_name();
}

template <typename Interface_T, bool is_move_only_T, std::size_t size_T, std::size_t alignment_T>
    requires(size_T != 0)
auto EraseMechanism<Interface_T, is_move_only_T, size_T, alignment_T>::
    uses_small_buffer() const noexcept -> bool
{
    return m_vtable.get().uses_small_buffer();
}

template <typename Interface_T, bool is_move_only_T, std::size_t size_T, std::size_t alignment_T>
    requires(size_T != 0)
constexpr auto EraseMechanism<Interface_T, is_move_only_T, size_T, alignment_T>::swap(
    std::pmr::polymorphic_allocator<>& lhs_allocator,
    Storage&                           lhs_storage,
    std::pmr::polymorphic_allocator<>& rhs_allocator,
    Storage&                           rhs_storage,
    const EraseMechanism&              rhs_erase_mechanism
) const -> void
{
    m_vtable.get()
        .swap(lhs_allocator, lhs_storage, rhs_allocator, rhs_storage, rhs_erase_mechanism);
}

template <typename Interface_T, bool is_move_only_T, std::size_t size_T, std::size_t alignment_T>
    requires(size_T != 0)
auto EraseMechanism<Interface_T, is_move_only_T, size_T, alignment_T>::reset(
    std::pmr::polymorphic_allocator<>& allocator,
    Storage&                           storage
) const -> void
{
    m_vtable.get().reset(allocator, storage);
}

template <typename Interface_T, bool is_move_only_T, std::size_t alignment_T>
template <typename T>
constexpr EraseMechanism<Interface_T, is_move_only_T, 0, alignment_T>::EraseMechanism(
    std::in_place_type_t<T>
) noexcept
    requires(!is_move_only_T)
    : m_vtable{ VTable::template Operations<T>::vtable }
{
}

template <typename Interface_T, bool is_move_only_T, std::size_t alignment_T>
template <typename T>
constexpr EraseMechanism<Interface_T, is_move_only_T, 0, alignment_T>::EraseMechanism(
    std::in_place_type_t<T>
) noexcept
    requires(is_move_only_T)
#ifdef KILN_DEBUG
    : m_vtable{ DiagnosticVTable::template Operations<T>::vtable }
#endif
{
}

template <typename Interface_T, bool is_move_only_T, std::size_t alignment_T>
auto EraseMechanism<Interface_T, is_move_only_T, 0, alignment_T>::copy_construct(
    std::pmr::polymorphic_allocator<>& allocator,
    const Storage&                     source_storage
) const -> Storage
    requires(!is_move_only_T)
{
    return m_vtable.get().copy_construct(allocator, source_storage);
}

template <typename Interface_T, bool is_move_only_T, std::size_t alignment_T>
constexpr auto EraseMechanism<Interface_T, is_move_only_T, 0, alignment_T>::move_construct(
    Storage&& source_storage
) noexcept -> Storage
{
    return Storage{ .handle = std::exchange(source_storage.handle, nullptr) };
}

template <typename Interface_T, bool is_move_only_T, std::size_t alignment_T>
auto EraseMechanism<Interface_T, is_move_only_T, 0, alignment_T>::move_construct(
    std::pmr::polymorphic_allocator<>&       destination_allocator,
    const std::pmr::polymorphic_allocator<>& source_allocator,
    Storage&&                                source_storage
) -> Storage
    requires(!is_move_only_T)
{
    if (destination_allocator == source_allocator)
    {
        return Storage{ .handle = std::exchange(source_storage.handle, nullptr) };
    }

    return copy_construct(destination_allocator, source_storage);
}

template <typename Interface_T, bool is_move_only_T, std::size_t alignment_T>
auto EraseMechanism<Interface_T, is_move_only_T, 0, alignment_T>::move_construct(
    [[maybe_unused]] const std::pmr::polymorphic_allocator<>& destination_allocator,
    [[maybe_unused]] const std::pmr::polymorphic_allocator<>& source_allocator,
    Storage&&                                                 source_storage
) noexcept -> Storage
    requires is_move_only_T
{
    PRECOND(destination_allocator == source_allocator);
    return Storage{ .handle = std::exchange(source_storage.handle, nullptr) };
}

template <typename Interface_T, bool is_move_only_T, std::size_t alignment_T>
auto EraseMechanism<Interface_T, is_move_only_T, 0, alignment_T>::drop(
    std::pmr::polymorphic_allocator<>& allocator,
    Storage&                           storage
) -> void
{
    if (storage.handle != nullptr)
    {
        allocator.delete_object(storage.handle);
    }
}

template <typename Interface_T, bool is_move_only_T, std::size_t alignment_T>
template <typename T, typename... Args_T>
auto EraseMechanism<Interface_T, is_move_only_T, 0, alignment_T>::construct(
    std::allocator_arg_t,
    std::pmr::polymorphic_allocator<>& allocator,
    std::in_place_type_t<T>,
    Args_T&&... args
) -> Storage
{
    return Storage{
        .handle = allocator.new_object<T>(std::forward<Args_T>(args)...),
    };
}

template <typename Interface_T, bool is_move_only_T, std::size_t alignment_T>
auto EraseMechanism<Interface_T, is_move_only_T, 0, alignment_T>::copy_assign(
    std::pmr::polymorphic_allocator<>& destination_allocator,
    Storage&                           destination_storage,
    const EraseMechanism&              destination_erase_mechanism,
    const Storage&                     source_storage
) const -> void
    requires(!is_move_only_T)
{
    m_vtable.get().copy_assign(
        destination_allocator,
        destination_storage,
        destination_erase_mechanism,
        source_storage
    );
}

template <typename Interface_T, bool is_move_only_T, std::size_t alignment_T>
constexpr auto EraseMechanism<Interface_T, is_move_only_T, 0, alignment_T>::address_of(
    Storage& storage
) -> Interface_T*
{
    PRECOND(storage.handle != nullptr);
    return storage.handle;
}

template <typename Interface_T, bool is_move_only_T, std::size_t alignment_T>
constexpr auto EraseMechanism<Interface_T, is_move_only_T, 0, alignment_T>::address_of(
    const Storage& storage
) -> const Interface_T*
{
    PRECOND(storage.handle != nullptr);
    return storage.handle;
}

template <typename Interface_T, bool is_move_only_T, std::size_t alignment_T>
constexpr auto EraseMechanism<Interface_T, is_move_only_T, 0, alignment_T>::dereference(
    Storage& storage
) -> Interface_T&
{
    PRECOND(storage.handle != nullptr);
    return *storage.handle;
}

template <typename Interface_T, bool is_move_only_T, std::size_t alignment_T>
constexpr auto EraseMechanism<Interface_T, is_move_only_T, 0, alignment_T>::dereference(
    const Storage& storage
) -> const Interface_T&
{
    PRECOND(storage.handle != nullptr);
    return *storage.handle;
}

template <typename Interface_T, bool is_move_only_T, std::size_t alignment_T>
auto EraseMechanism<Interface_T, is_move_only_T, 0, alignment_T>::type_hash() const
    -> uint64_t
{
    return m_vtable.get().type_hash();
}

template <typename Interface_T, bool is_move_only_T, std::size_t alignment_T>
auto EraseMechanism<Interface_T, is_move_only_T, 0, alignment_T>::type_name() const
    -> std::string_view
{
    return m_vtable.get().type_name();
}

template <typename Interface_T, bool is_move_only_T, std::size_t alignment_T>
constexpr auto EraseMechanism<Interface_T, is_move_only_T, 0, alignment_T>::swap(
    [[maybe_unused]] const std::pmr::polymorphic_allocator<>& lhs_allocator,
    Storage&                                                  lhs_storage,
    [[maybe_unused]] const std::pmr::polymorphic_allocator<>& rhs_allocator,
    Storage&                                                  rhs_storage,
    const EraseMechanism&
) -> void
{
    PRECOND(lhs_allocator == rhs_allocator);
    std::swap(lhs_storage.handle, rhs_storage.handle);
}

template <typename Interface_T, bool is_move_only_T, std::size_t alignment_T>
auto EraseMechanism<Interface_T, is_move_only_T, 0, alignment_T>::reset(
    std::pmr::polymorphic_allocator<>& allocator,
    Storage&                           storage
) -> void
{
    if (storage.handle != nullptr)
    {
        allocator.delete_object(storage.handle);
        storage.handle = nullptr;
    }
}

template <decayed_c T, typename Polymorphic_T>
    requires storable_in_polymorphic_c<T, std::remove_cvref_t<Polymorphic_T>>
auto any_cast(Polymorphic_T&& polymorphic) -> forward_like_t<T, Polymorphic_T>
{
    PRECOND(
        polymorphic.Polymorphic::m_erase_mechanism.type_hash() == util::hash_u64<T>(),
        std::format(
            "`Polymorphic` has type {}, but requested type is {}",
            polymorphic.Polymorphic::m_erase_mechanism.type_name(),
            util::name_of<T>()
        )
    );

    return static_cast<forward_like_t<T, Polymorphic_T>>(
        polymorphic.Polymorphic::m_erase_mechanism
            .dereference(polymorphic.Polymorphic::m_storage)
    );
}

template <decayed_c T, typename Polymorphic_T>
    requires std::derived_from<std::remove_cvref_t<Polymorphic_T>, internal::PolymorphicBase>
auto reinterpret_any_cast(Polymorphic_T&& polymorphic) -> forward_like_t<T, Polymorphic_T>
{
    return reinterpret_cast<forward_like_t<T, Polymorphic_T>>(
        polymorphic.Polymorphic::m_erase_mechanism
            .dereference(polymorphic.Polymorphic::m_storage)
    );
}

template <
    interface_c Interface_T,
    bool        is_move_only_T,
    std::size_t size_T,
    std::size_t alignment_T>
consteval auto
    Polymorphic<Interface_T, is_move_only_T, size_T, alignment_T>::is_move_only() -> bool
{
    return is_move_only_T;
}

template <
    interface_c Interface_T,
    bool        is_move_only_T,
    std::size_t size_T,
    std::size_t alignment_T>
consteval auto Polymorphic<Interface_T, is_move_only_T, size_T, alignment_T>::size()
    -> std::size_t
{
    return size_T;
}

template <
    interface_c Interface_T,
    bool        is_move_only_T,
    std::size_t size_T,
    std::size_t alignment_T>
consteval auto Polymorphic<Interface_T, is_move_only_T, size_T, alignment_T>::alignment()
    -> std::size_t
{
    return alignment_T;
}

template <
    interface_c Interface_T,
    bool        is_move_only_T,
    std::size_t size_T,
    std::size_t alignment_T>
Polymorphic<Interface_T, is_move_only_T, size_T, alignment_T>::Polymorphic(
    const Polymorphic& other
)
    requires(!is_move_only_T)
    : Polymorphic{ other, other.m_allocator }
{
}

template <
    interface_c Interface_T,
    bool        is_move_only_T,
    std::size_t size_T,
    std::size_t alignment_T>
Polymorphic<Interface_T, is_move_only_T, size_T, alignment_T>::Polymorphic(
    const Polymorphic&    other,
    const allocator_type& allocator
)
    requires(!is_move_only_T)
    : m_allocator{ allocator },
      m_storage{ other.m_erase_mechanism.copy_construct(m_allocator, other.m_storage) },
      m_erase_mechanism{ other.m_erase_mechanism }
{
}

template <
    interface_c Interface_T,
    bool        is_move_only_T,
    std::size_t size_T,
    std::size_t alignment_T>
Polymorphic<Interface_T, is_move_only_T, size_T, alignment_T>::Polymorphic(
    Polymorphic&& other
) noexcept
    : m_allocator{ other.m_allocator },
      m_storage{ other.m_erase_mechanism.move_construct(std::move(other.m_storage)) },
      m_erase_mechanism{ other.m_erase_mechanism }
{
}

template <
    interface_c Interface_T,
    bool        is_move_only_T,
    std::size_t size_T,
    std::size_t alignment_T>
Polymorphic<Interface_T, is_move_only_T, size_T, alignment_T>::Polymorphic(
    Polymorphic&&         other,
    const allocator_type& allocator
)
    : m_allocator{ allocator },
      m_storage{
          other.m_erase_mechanism.move_construct(
              m_allocator,
              other.m_allocator,
              std::move(other.m_storage)
          )   //
      },
      m_erase_mechanism{ other.m_erase_mechanism }
{
}

template <
    interface_c Interface_T,
    bool        is_move_only_T,
    std::size_t size_T,
    std::size_t alignment_T>
Polymorphic<Interface_T, is_move_only_T, size_T, alignment_T>::~Polymorphic()
{
    m_erase_mechanism.drop(m_allocator, m_storage);
}

template <
    interface_c Interface_T,
    bool        is_move_only_T,
    std::size_t size_T,
    std::size_t alignment_T>
template <typename T>
Polymorphic<Interface_T, is_move_only_T, size_T, alignment_T>::Polymorphic(T&& value)
    requires(!strips_to_c<T, Polymorphic>)
         && (!util::specialization_of_c<std::remove_cvref_t<T>, std::in_place_type_t>)
         && std::constructible_from<std::remove_cvref_t<T>, T&&>
         && storable_in_polymorphic_c<std::remove_cvref_t<T>, Polymorphic>

    : Polymorphic{
          std::allocator_arg,
          std::pmr::get_default_resource(),
          std::forward<T>(value),
      }
{
}

template <
    interface_c Interface_T,
    bool        is_move_only_T,
    std::size_t size_T,
    std::size_t alignment_T>
template <typename T>
Polymorphic<Interface_T, is_move_only_T, size_T, alignment_T>::Polymorphic(
    std::allocator_arg_t,
    const allocator_type& allocator,
    T&&                   value
)
    requires(!strips_to_c<T, Polymorphic>)
         && (!util::specialization_of_c<std::remove_cvref_t<T>, std::in_place_type_t>)
         && std::constructible_from<std::remove_cvref_t<T>, T&&>
         && storable_in_polymorphic_c<std::remove_cvref_t<T>, Polymorphic>
    : Polymorphic{
          std::allocator_arg,
          allocator,
          std::in_place_type<std::remove_cvref_t<T>>,
          std::forward<T>(value),
      }
{
}

template <
    interface_c Interface_T,
    bool        is_move_only_T,
    std::size_t size_T,
    std::size_t alignment_T>
template <typename T, typename... Args_T>
Polymorphic<Interface_T, is_move_only_T, size_T, alignment_T>::Polymorphic(
    std::in_place_type_t<T> in_place_type,
    Args_T&&... args
)
    requires std::constructible_from<T, Args_T&&...>
          && storable_in_polymorphic_c<T, Polymorphic>
    : Polymorphic{
          std::allocator_arg,
          std::pmr::get_default_resource(),
          in_place_type,
          std::forward<Args_T>(args)...,
      }
{
}

template <
    interface_c Interface_T,
    bool        is_move_only_T,
    std::size_t size_T,
    std::size_t alignment_T>
template <typename T, typename... Args_T>
Polymorphic<Interface_T, is_move_only_T, size_T, alignment_T>::Polymorphic(
    std::allocator_arg_t,
    const allocator_type&   allocator,
    std::in_place_type_t<T> in_place_type,
    Args_T&&... args
)
    requires std::constructible_from<T, Args_T&&...>
              && storable_in_polymorphic_c<T, Polymorphic>
    : m_allocator{ allocator },
      m_storage{
          EraseMechanism<Interface_T, is_move_only_T, size_T, alignment_T>::construct(
              std::allocator_arg,
              m_allocator,
              in_place_type,
              std::forward<Args_T>(args)...
          )   //
      },
      m_erase_mechanism{ std::in_place_type<T> }
{
}

template <
    interface_c Interface_T,
    bool        is_move_only_T,
    std::size_t size_T,
    std::size_t alignment_T>
auto Polymorphic<Interface_T, is_move_only_T, size_T, alignment_T>::operator=(
    const Polymorphic& other
) -> Polymorphic&
    requires(!is_move_only_T)
{
    if (this == &other)
    {
        return *this;
    }

    other.m_erase_mechanism
        .copy_assign(m_allocator, m_storage, m_erase_mechanism, other.m_storage);
    m_erase_mechanism = other.m_erase_mechanism;

    return *this;
}

template <
    interface_c Interface_T,
    bool        is_move_only_T,
    std::size_t size_T,
    std::size_t alignment_T>
// NOLINTNEXTLINE(*-noexcept-move-operations,*-noexcept-move,*-noexcept-move-constructor)
auto Polymorphic<Interface_T, is_move_only_T, size_T, alignment_T>::operator=(
    Polymorphic&& other
) -> Polymorphic&
    requires(!is_move_only_T)
{
    if (this == &other)
    {
        return *this;
    }

    if (m_allocator != other.m_allocator)
    {
        operator=(other);
        return *this;
    }

    swap(other);
    other.reset();

    return *this;
}

template <
    interface_c Interface_T,
    bool        is_move_only_T,
    std::size_t size_T,
    std::size_t alignment_T>
auto Polymorphic<Interface_T, is_move_only_T, size_T, alignment_T>::operator=(
    Polymorphic&& other
) noexcept -> Polymorphic&
    requires is_move_only_T
{
    if (this == &other)
    {
        return *this;
    }

    swap(other);
    other.reset();

    return *this;
}

template <
    interface_c Interface_T,
    bool        is_move_only_T,
    std::size_t size_T,
    std::size_t alignment_T>
auto Polymorphic<Interface_T, is_move_only_T, size_T, alignment_T>::operator->() noexcept
    -> Interface_T*
{
    return m_erase_mechanism.address_of(m_storage);
}

template <
    interface_c Interface_T,
    bool        is_move_only_T,
    std::size_t size_T,
    std::size_t alignment_T>
auto Polymorphic<Interface_T, is_move_only_T, size_T, alignment_T>::operator->() const noexcept
    -> const Interface_T*
{
    return m_erase_mechanism.address_of(m_storage);
}

template <
    interface_c Interface_T,
    bool        is_move_only_T,
    std::size_t size_T,
    std::size_t alignment_T>
auto Polymorphic<Interface_T, is_move_only_T, size_T, alignment_T>::operator*() noexcept
    -> Interface_T&
{
    return m_erase_mechanism.dereference(m_storage);
}

template <
    interface_c Interface_T,
    bool        is_move_only_T,
    std::size_t size_T,
    std::size_t alignment_T>
auto Polymorphic<Interface_T, is_move_only_T, size_T, alignment_T>::operator*() const noexcept
    -> const Interface_T&
{
    return m_erase_mechanism.dereference(m_storage);
}

template <
    interface_c Interface_T,
    bool        is_move_only_T,
    std::size_t size_T,
    std::size_t alignment_T>
auto Polymorphic<Interface_T, is_move_only_T, size_T, alignment_T>::
    get_allocator() const noexcept -> allocator_type
{
    return m_allocator;
}

// NOLINTNEXTLINE(*-noexcept-swap)
template <
    interface_c Interface_T,
    bool        is_move_only_T,
    std::size_t size_T,
    std::size_t alignment_T>
auto Polymorphic<Interface_T, is_move_only_T, size_T, alignment_T>::swap(
    Polymorphic& other
) -> void
{
    m_erase_mechanism.swap(
        m_allocator,
        m_storage,
        other.m_allocator,
        other.m_storage,
        other.m_erase_mechanism
    );
    std::swap(m_erase_mechanism, other.m_erase_mechanism);
}

template <
    interface_c Interface_T,
    bool        is_move_only_T,
    std::size_t size_T,
    std::size_t alignment_T>
auto Polymorphic<Interface_T, is_move_only_T, size_T, alignment_T>::reset() -> void
{
    m_erase_mechanism.reset(m_allocator, m_storage);
}

// ReSharper disable once CppSpecialFunctionWithoutNoexceptSpecification
// NOLINTNEXTLINE(*-noexcept-swap)
template <typename Interface_T, bool is_move_only_T, std::size_t size_T, std::size_t alignment_T>
auto swap(
    Polymorphic<Interface_T, is_move_only_T, size_T, alignment_T>& lhs,
    Polymorphic<Interface_T, is_move_only_T, size_T, alignment_T>& rhs
) -> void
{
    lhs.swap(rhs);
}

}   // namespace kiln::util
