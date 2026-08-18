module;

#include <filesystem>
#include <optional>

#include <fastgltf/core.hpp>

#include "kiln/util/contract_macros.hpp"

module kiln.gfx.asset.gltf.Parser;

import kiln.util.contracts;

namespace kiln::gfx::asset::gltf {

[[nodiscard]]
auto make_options(const bool generate_indices) noexcept -> fastgltf::Options
{
    fastgltf::Options result{ fastgltf::Options::LoadExternalBuffers };

    if (generate_indices)
    {
        result |= fastgltf::Options::GenerateMeshIndices;
    }

    return result;
}

auto Parser::load(const std::filesystem::path& filepath, const bool generate_indices)
    -> std::optional<fastgltf::Asset>
{
    fastgltf::GltfFileStream file{ filepath };
    if (!file.isOpen())
    {
        return std::nullopt;
    }

    fastgltf::Expected<fastgltf::Asset> asset{
        m_parser.loadGltf(file, filepath.parent_path(), make_options(generate_indices)),
    };
    if (asset.error() != fastgltf::Error::None)
    {
        return std::nullopt;
    }

    PRECOND(fastgltf::validate(asset.get()) == fastgltf::Error::None);

    return std::move(asset.get());
}

}   // namespace kiln::gfx::asset::gltf
