module;

#include <algorithm>
#include <optional>
#include <ranges>

#include <glm/common.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <fastgltf/tools.hpp>

#include <kiln/util/contract_macros.hpp>

module examples.frustum_culling.gltf_utils;

import kiln.util.contracts;

import examples.frustum_culling.AABB;

namespace demo {

[[nodiscard]]
auto apply_vulkan_basis_correction_on_position(const glm::vec3& vector) -> glm::vec3
{
    return glm::vec3{
        vector.x,
        vector.y * -1.f,
        vector.z * -1.f,
    };
}

auto apply_vulkan_basis_correction_on_transform(const glm::mat4x4& transform)
    -> glm::mat4x4
{
    static const glm::mat4x4 vulkan_basis_correction{
        glm::scale(glm::identity<glm::mat4x4>(), glm::vec3{ 1, -1, -1 })
    };

    return vulkan_basis_correction * transform * vulkan_basis_correction;
}

[[nodiscard]]
auto vec3_from(const fastgltf::AccessorBoundsArray& position_accessor_bound) -> glm::vec3
{
    PRECOND(
        position_accessor_bound.type()
        == fastgltf::AccessorBoundsArray::BoundsType::float64
    );
    PRECOND(position_accessor_bound.size() == 3);

    return glm::vec3{
        static_cast<float>(position_accessor_bound.get<double>(0)),
        static_cast<float>(position_accessor_bound.get<double>(1)),
        static_cast<float>(position_accessor_bound.get<double>(2)),
    };
}

auto aabb_from_position_accessor(const fastgltf::Accessor& position_accessor) -> AABB<>
{
    if (!position_accessor.min.has_value())
    {
        throw std::runtime_error{
            "Invalid glTF: POSITION accessor MUST have \"min\" defined"
        };
    }
    if (!position_accessor.max.has_value())
    {
        throw std::runtime_error{
            "Invalid glTF: POSITION accessor MUST have \"max\" defined"
        };
    }

    const glm::vec3 corrected_min{
        apply_vulkan_basis_correction_on_position(vec3_from(*position_accessor.min))
    };
    const glm::vec3 corrected_max{
        apply_vulkan_basis_correction_on_position(vec3_from(*position_accessor.max))
    };

    return AABB{
        .min = glm::min(corrected_min, corrected_max),
        .max = glm::max(corrected_min, corrected_max),
    };
}

template <typename Repr_T>
[[nodiscard]]
auto merge(const AABB<Repr_T>& lhs, const AABB<Repr_T>& rhs) -> AABB<Repr_T>
{
    return AABB<Repr_T>{
        .min = glm::min(lhs.min, rhs.min),
        .max = glm::max(lhs.max, rhs.max),
    };
}

template <typename Repr_T>
[[nodiscard]]
auto operator*(const glm::mat<4, 4, Repr_T>& transform, const AABB<Repr_T>& aabb)
    -> AABB<Repr_T>
{
    const std::array corners{
        aabb.min,
        glm::vec<3, Repr_T>{ aabb.max.x, aabb.min.y, aabb.min.z },
        glm::vec<3, Repr_T>{ aabb.min.x, aabb.max.y, aabb.min.z },
        glm::vec<3, Repr_T>{ aabb.min.x, aabb.min.y, aabb.max.z },
        glm::vec<3, Repr_T>{ aabb.max.x, aabb.max.y, aabb.min.z },
        glm::vec<3, Repr_T>{ aabb.max.x, aabb.min.y, aabb.max.z },
        glm::vec<3, Repr_T>{ aabb.min.x, aabb.max.y, aabb.max.z },
        aabb.max
    };

    return *std::ranges::fold_left_first(
        std::views::transform(
            corners,
            [&transform](const glm::vec<3, Repr_T>& corner) -> AABB<Repr_T>
            {
                const glm::vec<4, Repr_T> transformed_corner{
                    transform * glm::vec<4, Repr_T>{ corner, 1.f }
                };
                return AABB<Repr_T>{
                    .min = transformed_corner,
                    .max = transformed_corner,
                };
            }
        ),
        [](const AABB<Repr_T>& lhs, const AABB<Repr_T>& rhs) -> AABB<Repr_T>
        {
            return merge(lhs, rhs);   //
        }
    );
}

auto aabb_of(const fastgltf::Asset& model, const size_t scene_index)
    -> std::optional<AABB<>>
{
    std::optional<AABB<>> result{};

    fastgltf::iterateSceneNodes(
        model,
        scene_index,
        fastgltf::math::fmat4x4{},
        [&model, &result](
            const fastgltf::Node&          node,
            const fastgltf::math::fmat4x4& transform
        ) -> void
        {
            if (!node.meshIndex.has_value())
            {
                return;
            }

            for (
                const fastgltf::Primitive& primitive :
                model.meshes[*node.meshIndex].primitives
            )
            {
                const fastgltf::Attribute* position_attribute{
                    primitive.findAttribute("POSITION")
                };
                if (position_attribute == primitive.attributes.cend())
                {
                    continue;
                }

                const fastgltf::Accessor& position_accessor{
                    model.accessors[position_attribute->accessorIndex]
                };
                if (!position_accessor.min.has_value())
                {
                    throw std::runtime_error{
                        "Invalid glTF: POSITION accessor MUST have \"min\" defined"
                    };
                }
                if (!position_accessor.max.has_value())
                {
                    throw std::runtime_error{
                        "Invalid glTF: POSITION accessor MUST have \"max\" defined"
                    };
                }

                if (const AABB aabb{
                        apply_vulkan_basis_correction_on_transform(
                            glm::make_mat4(transform.data())
                        ) * aabb_from_position_accessor(position_accessor),
                    };
                    !result.has_value())
                {
                    result = aabb;
                }
                else
                {
                    result = merge(*result, aabb);
                }
            }
        }
    );

    return result;
}

}   // namespace demo
