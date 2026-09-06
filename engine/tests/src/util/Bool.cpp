#include <optional>

#include <catch2/catch_test_macros.hpp>

import kiln.util.Bool;
import kiln.util.containers.OptionalRef;

namespace kiln::util {

TEST_CASE("kiln::util::Bool")
{
    SECTION("transform to std::optional")
    {
        {
            constexpr Bool boolean{};
            [[maybe_unused]]
            auto optional{ boolean.transform([] -> float { return {}; }) };

            STATIC_REQUIRE(std::is_same_v<decltype(optional), std::optional<float>>);
        }

        SECTION("with value")
        {
            constexpr Bool         boolean{ true };
            constexpr static float other_value{ 3.2f };
            constexpr auto         optional{
                boolean.transform([] -> float { return other_value; })
            };

            STATIC_REQUIRE(optional.value() == other_value);
        }

        SECTION("without value")
        {
            constexpr Bool         boolean{};
            constexpr static float other_value{ 3.2f };
            constexpr auto         optional{
                boolean.transform([] -> float { return other_value; })
            };

            STATIC_REQUIRE(!optional.has_value());
        }
    }

    SECTION("transform to OptionalRef")
    {
        {
            constexpr Bool         boolean{};
            constexpr static float other_value{ 3.2f };
            [[maybe_unused]]
            auto optional{ boolean.transform([] -> const float& { return other_value; }) };

            STATIC_REQUIRE(std::is_same_v<decltype(optional), OptionalRef<const float>>);
        }

        SECTION("with value")
        {
            constexpr Bool         boolean{ true };
            constexpr static float other_value{ 3.2f };
            constexpr auto         optional{
                boolean.transform([] -> const float& { return other_value; })
            };

            STATIC_REQUIRE(*optional == other_value);
        }

        SECTION("without value")
        {
            constexpr Bool         boolean{};
            constexpr static float other_value{ 3.2f };
            constexpr auto         optional{
                boolean.transform([] -> const float& { return other_value; })
            };

            STATIC_REQUIRE(!optional.has_value());
        }
    }
}

}   // namespace kiln::util
