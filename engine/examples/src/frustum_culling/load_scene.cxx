module;

#include <optional>
#include <ranges>

#include <glm/ext/matrix_transform.hpp>
#include <glm/vec3.hpp>

#include <fastgltf/core.hpp>

module examples.frustum_culling.load_scene;

import kiln.util.Lazy;

import examples.frustum_culling.AABB;
import examples.frustum_culling.gltf_utils;
import examples.frustum_culling.workflow.load_scene;
import examples.frustum_culling.workflow.ModelDescription;

namespace demo {

template <typename Repr_T>
[[nodiscard]]
auto extent_of(const AABB<Repr_T>& aabb) -> glm::vec<3, Repr_T>
{
    return aabb.max - aabb.min;
}

[[nodiscard]]
auto model_descriptions_from(
    const std::pmr::polymorphic_allocator<>& allocator,
    const fastgltf::Asset&                   model_asset,
    const std::size_t                        scene_index,
    const uint32_t                           grid_size
) -> std::pmr::vector<ModelDescription>
{
    std::pmr::vector<ModelDescription> result{ allocator };

    const std::optional<AABB<>> model_aabb{ aabb_of(model_asset, scene_index) };
    if (!model_aabb.has_value())
    {
        return result;
    }

    result.reserve(grid_size * grid_size * grid_size);
    const glm::vec3 distance{ extent_of(*model_aabb) * 2.f };
    const glm::vec3 start_offset{ -static_cast<float>(grid_size - 1) * distance / 2.f };
    for (
        const auto indices{ std::views::iota(0u, grid_size) };
        const auto [x, y, z] : std::views::cartesian_product(indices, indices, indices)
    )
    {
        const glm::vec3 offset{
            start_offset.x + static_cast<float>(x) * distance.x,
            start_offset.y + static_cast<float>(y) * distance.y,
            start_offset.z + static_cast<float>(z) * distance.z,
        };
        result.push_back(
            ModelDescription{
                .model_asset = model_asset,
                .scene_index = scene_index,
                .transform   = glm::translate(glm::identity<glm::mat4x4>(), offset),
            }
        );
    }

    return result;
}

[[nodiscard]]
auto load_scene(
    const kiln::gfx::renderer::Device&  device,
    kiln::gfx::renderer::Allocator&     gpu_allocator,
    kiln::gfx::renderer::StagingStream& staging_stream,
    kiln::gfx::asset::gltf::Parser&     model_parser,
    const std::filesystem::path&        model_filepath,
    const bool                          disable_culling,
    const uint32_t                      grid_size,
    std::pmr::memory_resource&          transient_memory_resource
) -> Scene
{
    const auto model_asset{
        model_parser.load(model_filepath, true)
            .value_or(
                kiln::util::Lazy{
                    [&model_filepath] [[noreturn]]
                        -> decltype(model_parser.load(model_filepath))::value_type
                    {
                        throw std::runtime_error{
                            std::format(
                                "Model could not be loaded from {}",
                                model_filepath.generic_string()
                            ),
                        };
                    },
                }
            )
    };

    const std::size_t scene_index{
        model_asset.defaultScene.value_or(
            kiln::util::Lazy{
                [&model_filepath, &model_asset] -> std::size_t
                {
                    if (model_asset.scenes.empty())
                    {
                        throw std::runtime_error{
                            std::format(
                                "The provided glTF asset ({}) is a library"
                                " (it has no scenes)"
                                " and therefor cannot be loaded directly.",
                                model_filepath.generic_string()
                            ),
                        };
                    }
                    return 0;
                },
            }
        ),
    };

    return load_scene(
        model_descriptions_from(
            &transient_memory_resource,
            model_asset,
            scene_index,
            grid_size
        ),
        device,
        gpu_allocator,
        staging_stream,
        disable_culling,
        transient_memory_resource
    );
}

}   // namespace demo
