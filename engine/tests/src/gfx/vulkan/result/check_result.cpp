#include <expected>
#include <type_traits>
#include <variant>

#include <catch2/catch_test_macros.hpp>

import kiln.gfx.vulkan.result.check_result;
import kiln.gfx.vulkan.result.Result;
import kiln.gfx.vulkan.result.TypedResultCode;
import kiln.gfx.vulkan.result.VulkanError;
import kiln.util.contracts;

import vulkan;

namespace kiln::gfx::vulkan {

TEST_CASE("kiln::gfx::vulkan::check_result")
{
    SECTION("vk::Result")
    {
        SECTION("no expected result code")
        {
            static_assert(
                std::is_same_v<decltype(check_result(vk::Result::eSuccess)), void>
            );

#ifdef KILN_DEBUG
            REQUIRE_THROWS_AS(
                check_result(vk::Result::eErrorExtensionNotPresent),
                util::PreconditionViolation
            );
#endif

            REQUIRE_THROWS_AS(check_result(vk::Result::eErrorDeviceLost), VulkanError);

            check_result(vk::Result::eSuccess);
        }

        SECTION("expected result code")
        {
            static_assert(
                std::is_same_v<
                    decltype(check_result<vk::Result::eIncomplete>(vk::Result::eSuccess)),
                    std::variant<
                        TypedResultCode<vk::Result::eSuccess>,
                        TypedResultCode<vk::Result::eIncomplete>>>
            );

#ifdef KILN_DEBUG
            REQUIRE_THROWS_AS(
                check_result<vk::Result::eIncomplete>(
                    vk::Result::eErrorExtensionNotPresent
                ),
                util::PreconditionViolation
            );
#endif

            REQUIRE_THROWS_AS(
                check_result<vk::Result::eIncomplete>(vk::Result::eErrorDeviceLost),
                VulkanError
            );

            REQUIRE(
                std::holds_alternative<TypedResultCode<vk::Result::eSuccess>>(
                    check_result<vk::Result::eErrorDeviceLost>(vk::Result::eSuccess)
                )
            );

            REQUIRE(
                std::holds_alternative<TypedResultCode<vk::Result::eErrorDeviceLost>>(
                    check_result<vk::Result::eErrorDeviceLost>(
                        vk::Result::eErrorDeviceLost
                    )
                )
            );
        }
    }

    SECTION("vk::ResultValue<T>")
    {
        SECTION("no expected result code")
        {
            static_assert(
                std::is_same_v<
                    decltype(check_result(std::declval<vk::ResultValue<vk::Instance>>())),
                    vk::Instance>
            );

#ifdef KILN_DEBUG
            REQUIRE_THROWS_AS(
                check_result(
                    vk::ResultValue<vk::Instance>{
                        vk::Result::eErrorExtensionNotPresent,
                        nullptr,
                    }
                ),
                util::PreconditionViolation
            );
#endif

            REQUIRE_THROWS_AS(
                check_result(
                    vk::ResultValue<vk::Instance>{
                        vk::Result::eErrorDeviceLost,
                        nullptr,
                    }
                ),
                VulkanError
            );

            std::ignore = check_result(
                vk::ResultValue<vk::Instance>{
                    vk::Result::eSuccess,
                    nullptr,
                }
            );
        }

        SECTION("expected result code")
        {
            static_assert(
                std::is_same_v<
                    decltype(check_result<vk::Result::eIncomplete>(
                        std::declval<vk::ResultValue<vk::Instance>>()
                    )),

                    Result<vk::Instance, vk::Result::eSuccess, vk::Result::eIncomplete>>
            );

#ifdef KILN_DEBUG
            REQUIRE_THROWS_AS(
                check_result<vk::Result::eIncomplete>(vk::ResultValue<vk::Instance>{
                    vk::Result::eErrorExtensionNotPresent,
                    nullptr,
                }),
                util::PreconditionViolation
            );
#endif

            REQUIRE_THROWS_AS(
                check_result<vk::Result::eIncomplete>(vk::ResultValue<vk::Instance>{
                    vk::Result::eErrorDeviceLost,
                    nullptr,
                }),
                VulkanError
            );

            REQUIRE(
                std::holds_alternative<TypedResultCode<vk::Result::eSuccess>>(
                    check_result<vk::Result::eErrorDeviceLost>(
                        vk::ResultValue<vk::Instance>{
                            vk::Result::eSuccess,
                            nullptr,
                        }
                    )
                        .result_code()
                )
            );

            REQUIRE(
                std::holds_alternative<TypedResultCode<vk::Result::eErrorDeviceLost>>(
                    check_result<vk::Result::eErrorDeviceLost>(
                        vk::ResultValue<vk::Instance>{
                            vk::Result::eErrorDeviceLost,
                            nullptr,
                        }
                    )
                        .result_code()
                )
            );
        }
    }

    SECTION("std::expected<T, vk::Result>")
    {
        SECTION("no expected result code")
        {
            static_assert(std::is_same_v<
                          decltype(check_result(
                              std::declval<std::expected<vk::Instance, vk::Result>>()
                          )),
                          vk::Instance>);

#ifdef KILN_DEBUG
            REQUIRE_THROWS_AS(
                check_result(
                    std::expected<vk::Instance, vk::Result>{
                        std::unexpect,
                        vk::Result::eErrorExtensionNotPresent,
                    }
                ),
                util::PreconditionViolation
            );
#endif

            REQUIRE_THROWS_AS(
                check_result(
                    std::expected<vk::Instance, vk::Result>{
                        std::unexpect,
                        vk::Result::eErrorDeviceLost,
                    }
                ),
                VulkanError
            );

            std::ignore
                = check_result(std::expected<vk::Instance, vk::Result>{ nullptr });
        }

        SECTION("expected result code")
        {
            static_assert(
                std::is_same_v<
                    decltype(check_result<vk::Result::eErrorDeviceLost>(
                        std::declval<std::expected<vk::Instance, vk::Result>>()
                    )),
                    std::expected<
                        vk::Instance,
                        std::variant<TypedResultCode<vk::Result::eErrorDeviceLost>>>>
            );

#ifdef KILN_DEBUG
            REQUIRE_THROWS_AS(
                check_result<vk::Result::eIncomplete>(
                    std::expected<vk::Instance, vk::Result>{
                        std::unexpect,
                        vk::Result::eErrorExtensionNotPresent,
                    }
                ),
                util::PreconditionViolation
            );
#endif

            REQUIRE_THROWS_AS(
                check_result<vk::Result::eIncomplete>(
                    std::expected<vk::Instance, vk::Result>{
                        std::unexpect,
                        vk::Result::eErrorDeviceLost,
                    }
                ),
                VulkanError
            );

            REQUIRE(
                check_result<vk::Result::eErrorDeviceLost>(
                    std::expected<vk::Instance, vk::Result>{ nullptr }
                )
                    .has_value()
            );

            REQUIRE(
                !check_result<vk::Result::eErrorDeviceLost>(
                     std::expected<vk::Instance, vk::Result>{
                         std::unexpect,
                         vk::Result::eErrorDeviceLost,
                     }
                )
                     .has_value()   //
            );
        }
    }
}

}   // namespace kiln::gfx::vulkan
