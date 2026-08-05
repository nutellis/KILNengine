module;

#include <array>
#include <functional>
#include <memory_resource>
#include <optional>
#include <vector>

#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>

#include <fastgltf/core.hpp>

#include "kiln/util/memory/lifetimebound.hpp"

export module examples.frustum_culling.workflow.GltfModelLoader;

import kiln.gfx.renderer.memory.BufferRegion;
import kiln.gfx.renderer.memory.LazyCopy;
import kiln.util.containers.CopyableFunction;

import examples.frustum_culling.AABB;
import examples.frustum_culling.shaders;

namespace demo {

enum struct SupportedElementType : uint32_t
{
    eIndex,
    ePosition,
    eNormal,
    eTangent,
    eTexCoord0,
    eTexCoord1,
    eColor0,
    COUNT,
};

export class GltfModelLoader {
public:
    using allocator_type = std::pmr::polymorphic_allocator<>;


    GltfModelLoader(const GltfModelLoader&, const allocator_type&);
    GltfModelLoader(GltfModelLoader&&, const allocator_type&);

    // clang-format off
    explicit GltfModelLoader(
        kiln_lifetimebound const fastgltf::Asset& model,
        std::size_t                               scene_index,
        const glm::mat4x4&                        transform = glm::identity<glm::mat4x4>()
    );
    // clang-format on
    // clang-format off
    explicit GltfModelLoader(
        std::allocator_arg_t,
        const allocator_type&                     allocator,
        kiln_lifetimebound const fastgltf::Asset& model,
        std::size_t                               scene_index,
        const glm::mat4x4&                        transform = glm::identity<glm::mat4x4>()
    );
    // clang-format on


    [[nodiscard]]
    auto get_allocator() const noexcept -> allocator_type;

    [[nodiscard]]
    auto instance_count() const noexcept -> uint32_t;
    [[nodiscard]]
    auto geometry_buffer_size_bytes() const noexcept -> uint32_t;
    [[nodiscard]]
    auto materials_buffer_size_bytes() const noexcept -> uint32_t;
    [[nodiscard]]
    auto instance_buffer_size_bytes() const noexcept -> uint32_t;
    [[nodiscard]]
    auto draw_command_count() const noexcept -> uint32_t;

    [[nodiscard]]
    static auto geometry_buffer_alignment() noexcept -> uint32_t;
    [[nodiscard]]
    static auto materials_buffer_alignment() noexcept -> uint32_t;
    [[nodiscard]]
    static auto instance_buffer_alignment() noexcept -> uint32_t;

    auto set_instance_draw_command_index_offset(
        uint32_t instance_draw_command_index_offset
    ) -> void;
    auto set_geometry_buffer_byte_offset(uint32_t geometry_buffer_byte_offset) -> void;
    auto set_materials_buffer_byte_offset(uint32_t materials_buffer_byte_offset) -> void;
    auto set_instance_buffer_byte_offset(uint32_t instance_buffer_byte_offset) -> void;
    auto set_instance_index_offset(uint32_t instance_index_offset) -> void;

    [[nodiscard]]
    auto instance_draw_command_index_writer() const noexcept kiln_lifetimebound
        -> kiln::gfx::renderer::LazyCopy;
    [[nodiscard]]
    auto instance_sphere_bounding_volume_writer() const noexcept kiln_lifetimebound
        -> kiln::gfx::renderer::LazyCopy;
    [[nodiscard]]
    auto geometry_writer() const noexcept kiln_lifetimebound
        -> kiln::gfx::renderer::LazyCopy;
    [[nodiscard]]
    auto material_writer() const noexcept kiln_lifetimebound
        -> kiln::gfx::renderer::LazyCopy;
    [[nodiscard]]
    auto instance_writer() const noexcept kiln_lifetimebound
        -> kiln::gfx::renderer::LazyCopy;
    [[nodiscard]]
    auto draw_command_writer() const noexcept kiln_lifetimebound
        -> kiln::gfx::renderer::LazyCopy;

private:
    struct Offsets {
        std::optional<uint32_t> instance_draw_command_index_offset;
        std::optional<uint32_t> geometry_buffer_byte_offset;
        std::optional<uint32_t> materials_buffer_byte_offset;
        std::optional<uint32_t> instance_index_offset;
        std::optional<uint32_t> instance_buffer_byte_offset;
    };

    class Manifest {
    public:
        using allocator_type = std::pmr::polymorphic_allocator<>;


        Manifest(const Manifest&, const allocator_type&);
        Manifest(Manifest&&, const allocator_type&);

        explicit Manifest(
            const fastgltf::Asset& model,
            std::size_t            scene_index,
            const glm::mat4x4&     transform
        );
        explicit Manifest(
            std::allocator_arg_t,
            const allocator_type&  allocator,
            const fastgltf::Asset& model,
            std::size_t            scene_index,
            const glm::mat4x4&     transform
        );


        [[nodiscard]]
        auto get_allocator() const noexcept -> allocator_type;

        [[nodiscard]]
        auto instance_count() const noexcept -> uint32_t;
        [[nodiscard]]
        auto geometry_buffer_size_bytes() const noexcept -> uint32_t;
        [[nodiscard]]
        auto materials_buffer_size_bytes() const noexcept -> uint32_t;
        [[nodiscard]]
        auto instance_buffer_size_bytes() const noexcept -> uint32_t;
        [[nodiscard]]
        auto draw_command_count() const noexcept -> uint32_t;

        [[nodiscard]]
        static auto geometry_buffer_alignment() noexcept -> uint32_t;
        [[nodiscard]]
        static auto materials_buffer_alignment() noexcept -> uint32_t;
        [[nodiscard]]
        static auto instance_buffer_alignment() noexcept -> uint32_t;

        auto write_instance_draw_command_indices(
            const fastgltf::Asset& model,
            const Offsets&         global_offsets,
            std::span<std::byte>   out
        ) const -> void;
        auto write_instance_sphere_bounding_volumes(
            const fastgltf::Asset& model,
            std::span<std::byte>   out
        ) const -> void;
        auto write_geometry(const fastgltf::Asset& model, std::span<std::byte> out) const
            -> void;
        auto write_materials(const fastgltf::Asset& model, std::span<std::byte> out) const
            -> void;
        auto write_instances(const fastgltf::Asset& model, std::span<std::byte> out) const
            -> void;
        auto write_draw_commands(
            const fastgltf::Asset& model,
            const Offsets&         global_offsets,
            std::span<std::byte>   out
        ) const -> void;

    private:
        using Writer = kiln::util::CopyableFunction<
            auto(
                const fastgltf::Asset&,
                std::span<const uint32_t, std::to_underlying(SupportedElementType::COUNT)>,
                std::span<std::byte>
            ) const->void>;
        using DrawCommandWriter = kiln::util::CopyableFunction<
            auto(const fastgltf::Asset&, const Offsets&, std::span<std::byte>) const->void>;

        static auto preprocess(
            const fastgltf::Asset& model,
            std::size_t            scene_index,
            Manifest&              manifest
        ) -> void;

        static auto process(
            const fastgltf::Asset& model,
            const glm::mat4x4&     transform,
            const fastgltf::Node&  node,
            Manifest&              manifest
        ) -> void;
        static auto process(
            const fastgltf::Asset& model,
            const glm::mat4x4&     transform,
            std::size_t            mesh_index,
            Manifest&              manifest
        ) -> void;
        static auto process(
            const fastgltf::Asset&     model,
            const fastgltf::Primitive& primitive,
            Manifest&                  manifest
        ) -> void;

        template <typename Element_T>
        [[nodiscard]]
        static auto element_writer_from(
            SupportedElementType element_type,
            std::size_t          accessor_index,
            uint32_t             element_byte_offset
        ) -> Writer;
        [[nodiscard]]
        static auto element_writer_from(
            SupportedElementType   element_type,
            fastgltf::AccessorType accessor_type,
            std::size_t            accessor_index,
            uint32_t               element_byte_offset
        ) -> Writer;
        [[nodiscard]]
        static auto color_vec3_writer_from(
            SupportedElementType element_type,
            std::size_t          accessor_index,
            uint32_t             element_byte_offset
        ) -> Writer;


        [[nodiscard]]
        auto shader_primitive_from(
            const Offsets&             global_offsets,
            std::size_t                mesh_index,
            const fastgltf::Primitive& primitive
        ) const -> shaders::Primitive;
        [[nodiscard]]
        auto vertex_offset_from(const fastgltf::Primitive& primitive) const -> uint32_t;


        uint32_t m_geometry_buffer_size_bytes{};
        uint32_t m_materials_buffer_size_bytes{};
        uint32_t m_instance_count{};
        uint32_t m_draw_command_count{};

        std::array<uint32_t, std::to_underlying(SupportedElementType::COUNT)>
                                   m_element_buffer_byte_offsets{};
        std::pmr::vector<uint32_t> m_accessor_element_byte_offsets;
        std::pmr::vector<uint32_t> m_material_indices;
        std::pmr::vector<uint32_t> m_per_mesh_instance_offsets;
        std::pmr::vector<uint32_t> m_per_mesh_instance_counts;
        std::pmr::vector<uint32_t> m_per_mesh_draw_command_index_offsets;

        std::pmr::vector<Writer>            m_geometry_writers;
        std::pmr::vector<shaders::Material> m_materials;
        std::pmr::vector<glm::mat4x4>       m_transforms;
        std::pmr::vector<glm::mat3x3>       m_normal_matrices;
        std::pmr::vector<shaders::SBV>      m_transformed_sphere_bounding_volumes;
    };

    using DrawCommandWriterFactory =                                               //
        kiln::util::CopyableFunction<                                              //
            auto(const Offsets&) const noexcept -> kiln::gfx::renderer::LazyCopy   //
            >;


    std::reference_wrapper<const fastgltf::Asset> m_model;
    Manifest                                      m_manifest;
    Offsets                                       m_offsets;
};

}   // namespace demo
