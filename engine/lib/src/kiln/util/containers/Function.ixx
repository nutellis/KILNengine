module;

#include <concepts>
#include <cstddef>
#include <format>
#include <functional>
#include <type_traits>
#include <utility>

#include "kiln/util/contract_macros.hpp"

export module kiln.util.containers.Function;

import kiln.util.concepts.decayed;
import kiln.util.concepts.function;
import kiln.util.concepts.specialization_of;
import kiln.util.concepts.storable;
import kiln.util.concepts.strips_to;
import kiln.util.containers.SmallBuffer;
import kiln.util.contracts;
import kiln.util.memory.LifeCycleEraseMechanism;
import kiln.util.reflection;
import kiln.util.type_traits.const_like;
import kiln.util.type_traits.forward_like;

namespace kiln::util {

export consteval auto default_function_size() -> std::size_t
{
    return 3 * sizeof(void*);
}

export consteval auto default_function_alignment() -> std::size_t
{
    return alignof(std::max_align_t);
}

export template <
    function_c  Signature_T,
    bool        is_move_only_T = false,
    std::size_t size_T         = default_function_size(),
    std::size_t alignment_T    = default_function_alignment()>
class Function;

namespace internal {

class FunctionBase {};

template <typename T, typename Signature_T>
constexpr bool callable_with_qualifiers_v = false;

template <std::size_t size_T, std::size_t alignment_T>
using Storage = SmallBuffer<std::max(size_T, sizeof(void*)), alignment_T>;

}   // namespace internal

export template <typename T, typename Function_T>
concept storable_in_function_c
    = std::derived_from<Function_T, internal::FunctionBase>
   && decayed_c<T>
   && util::storable_c<T>
   && internal::callable_with_qualifiers_v<T, typename Function_T::Signature>
   // TODO: use std::copyable when lambdas become copy assignable
   && (Function_T::is_move_only() || std::copy_constructible<T>);

export template <decayed_c T, typename Function_T>
    requires storable_in_function_c<T, std::remove_cvref_t<Function_T>>
[[nodiscard]]
constexpr auto any_cast(Function_T&& function) -> forward_like_t<T, Function_T>;

export template <decayed_c T, typename Function_T>
    requires std::derived_from<std::remove_cvref_t<Function_T>, internal::FunctionBase>
[[nodiscard]]
constexpr auto reinterpret_any_cast(Function_T&& function)
    -> forward_like_t<T, Function_T>;

#ifdef KILN_TEMP_CONST
  #error KILN_TEMP_CONST should not be defined
#endif
#ifdef KILN_TEMP_REF
  #error KILN_TEMP_REF should not be defined
#endif
#ifdef KILN_TEMP_IS_NOEXCEPT
  #error KILN_TEMP_IS_NOEXCEPT should not be defined
#endif
#ifdef KILN_TEMP_NAMESPACE
  #error KILN_TEMP_NAMESPACE should not be defined
#endif

#define KILN_TEMP_NAMESPACE   internal_spec
#define KILN_TEMP_IS_NOEXCEPT false
#include "Function.impl.hpp"

#define KILN_TEMP_NAMESPACE   internal_spec_noexcept
#define KILN_TEMP_IS_NOEXCEPT true
#include "Function.impl.hpp"

#define KILN_TEMP_NAMESPACE   internal_spec_ref
#define KILN_TEMP_REF         &
#define KILN_TEMP_IS_NOEXCEPT false
#include "Function.impl.hpp"

#define KILN_TEMP_NAMESPACE   internal_spec_ref_noexcept
#define KILN_TEMP_REF         &
#define KILN_TEMP_IS_NOEXCEPT true
#include "Function.impl.hpp"

#define KILN_TEMP_NAMESPACE   internal_spec_refref
#define KILN_TEMP_REF         &&
#define KILN_TEMP_IS_NOEXCEPT false
#include "Function.impl.hpp"

#define KILN_TEMP_NAMESPACE   internal_spec_refref_noexcept
#define KILN_TEMP_REF         &&
#define KILN_TEMP_IS_NOEXCEPT true
#include "Function.impl.hpp"

#define KILN_TEMP_NAMESPACE   internal_spec_const
#define KILN_TEMP_CONST       const
#define KILN_TEMP_IS_NOEXCEPT false
#include "Function.impl.hpp"

#define KILN_TEMP_NAMESPACE   internal_spec_const_noexcept
#define KILN_TEMP_CONST       const
#define KILN_TEMP_IS_NOEXCEPT true
#include "Function.impl.hpp"

#define KILN_TEMP_NAMESPACE   internal_spec_const_ref
#define KILN_TEMP_CONST       const
#define KILN_TEMP_REF         &
#define KILN_TEMP_IS_NOEXCEPT false
#include "Function.impl.hpp"

#define KILN_TEMP_NAMESPACE   internal_spec_const_ref_noexcept
#define KILN_TEMP_CONST       const
#define KILN_TEMP_REF         &
#define KILN_TEMP_IS_NOEXCEPT true
#include "Function.impl.hpp"

#define KILN_TEMP_NAMESPACE   internal_spec_const_refref
#define KILN_TEMP_CONST       const
#define KILN_TEMP_REF         &&
#define KILN_TEMP_IS_NOEXCEPT false
#include "Function.impl.hpp"

#define KILN_TEMP_NAMESPACE   internal_spec_const_refref_noexcept
#define KILN_TEMP_CONST       const
#define KILN_TEMP_REF         &&
#define KILN_TEMP_IS_NOEXCEPT true
#include "Function.impl.hpp"

// ReSharper disable once CppSpecialFunctionWithoutNoexceptSpecification
// NOLINTNEXTLINE(*-noexcept-swap)
export template <
    typename Signature_T,
    bool        is_move_only_T,
    std::size_t size_T,
    std::size_t alignment_T>
constexpr auto swap(
    Function<Signature_T, is_move_only_T, size_T, alignment_T>& lhs,
    Function<Signature_T, is_move_only_T, size_T, alignment_T>& rhs
) -> void;

}   // namespace kiln::util

namespace kiln::util {

template <decayed_c T, typename Function_T>
    requires storable_in_function_c<T, std::remove_cvref_t<Function_T>>
constexpr auto any_cast(Function_T&& function) -> forward_like_t<T, Function_T>
{
    PRECOND(
        function.Function::m_erase_mechanism.type_hash() == util::hash_u64<T>(),
        std::format(
            "`Function` has type {}, but requested type is {}",
            function.Function::m_erase_mechanism.type_name(),
            util::name_of<T>()
        )
    );

    auto* const address{
        function.Function::m_erase_mechanism.address_of(function.Function::m_storage)
    };

    PRECOND(address != nullptr);

    return std::forward_like<Function_T>(
        // ReSharper disable once CppDFANullDereference
        *static_cast<const_like_t<T, std::remove_reference_t<Function_T>>*>(address)
    );
}

template <decayed_c T, typename Function_T>
    requires std::derived_from<std::remove_cvref_t<Function_T>, internal::FunctionBase>
constexpr auto reinterpret_any_cast(Function_T&& function)
    -> forward_like_t<T, Function_T>
{
    auto* const address{
        function.Function::m_erase_mechanism.address_of(function.Function::m_storage)
    };

    PRECOND(address != nullptr);

    return std::forward_like<Function_T>(
        // ReSharper disable once CppDFANullDereference
        *reinterpret_cast<const_like_t<T, std::remove_reference_t<Function_T>>*>(address)
    );
}

// ReSharper disable once CppSpecialFunctionWithoutNoexceptSpecification
// NOLINTNEXTLINE(*-noexcept-swap)
template <typename Signature_T, bool is_move_only_T, std::size_t size_T, std::size_t alignment_T>
constexpr auto swap(
    Function<Signature_T, is_move_only_T, size_T, alignment_T>& lhs,
    Function<Signature_T, is_move_only_T, size_T, alignment_T>& rhs
) -> void
{
    lhs.swap(rhs);
}

}   // namespace kiln::util
