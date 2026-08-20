module;

#include <array>
#include <exception>
#include <type_traits>

export module kiln.util.ScopeFail;

import kiln.util.concepts.storable;

namespace kiln::util {

export template <storable_c Rollback_T>
    requires(std::is_nothrow_invocable_v<Rollback_T>)
class [[nodiscard]]
ScopeFail {
public:
    ScopeFail(const ScopeFail&) = delete;
    ScopeFail(ScopeFail&&)      = default;
    constexpr ~ScopeFail();

    constexpr explicit(false) ScopeFail(const Rollback_T& rollback) noexcept
        requires(std::is_nothrow_constructible_v<Rollback_T, const Rollback_T&>);
    constexpr explicit(false) ScopeFail(Rollback_T&& rollback) noexcept
        requires(std::is_nothrow_constructible_v<Rollback_T, Rollback_T &&>);

private:
    Rollback_T m_rollback;
    int        m_uncaught_exceptions{
               []
               {
#ifdef __cpp_constexpr_exceptions
            static_assert(false, "FIXME: Exceptions are now constexpr");
#endif
            return std::is_constant_evaluated() ? 0 : std::uncaught_exceptions();
        }()
    };
};

}   // namespace kiln::util

namespace kiln::util {

template <storable_c Rollback_T>
    requires(std::is_nothrow_invocable_v<Rollback_T>)
constexpr ScopeFail<Rollback_T>::~ScopeFail<Rollback_T>()
{
#ifdef __cpp_constexpr_exceptions
    static_assert(false, "FIXME: Exceptions are now constexpr");
#endif
    if !consteval
    {
        if (m_uncaught_exceptions < std::uncaught_exceptions())
        {
            std::invoke(m_rollback);
        }
    }
}

template <storable_c Rollback_T>
    requires(std::is_nothrow_invocable_v<Rollback_T>)
constexpr ScopeFail<Rollback_T>::ScopeFail(const Rollback_T& rollback) noexcept
    requires(std::is_nothrow_constructible_v<Rollback_T, const Rollback_T&>)
    : m_rollback{ rollback }
{
}

template <storable_c Rollback_T>
    requires(std::is_nothrow_invocable_v<Rollback_T>)
constexpr ScopeFail<Rollback_T>::ScopeFail(Rollback_T&& rollback) noexcept
    requires(std::is_nothrow_constructible_v<Rollback_T, Rollback_T &&>)
    : m_rollback{ std::move(rollback) }
{
}

}   // namespace kiln::util
