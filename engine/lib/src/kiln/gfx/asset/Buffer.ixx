module;

#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <span>

export module kiln.gfx.asset.Buffer;

import kiln.util.memory.Deleter;

namespace kiln::gfx::asset {

export class Buffer {
public:
    using allocator_type = std::pmr::polymorphic_allocator<>;


    Buffer(const Buffer& other) = delete ("buffer might be huge");
    Buffer(Buffer&& other) noexcept;
    Buffer(Buffer&& other, const allocator_type& allocator);
    ~Buffer();

    explicit Buffer(uint64_t size);
    explicit Buffer(std::allocator_arg_t, const allocator_type& allocator, uint64_t size);


    auto operator=(const Buffer&) -> Buffer& = delete ("buffer might be huge");
    // ReSharper disable once CppSpecialFunctionWithoutNoexceptSpecification
    auto operator=(Buffer&&) -> Buffer&;


    [[nodiscard]]
    auto get_allocator() const noexcept -> allocator_type;
    [[nodiscard]]
    auto bytes() noexcept -> std::span<std::byte>;
    [[nodiscard]]
    auto bytes() const noexcept -> std::span<const std::byte>;

    auto reset() -> void;
    // ReSharper disable once CppSpecialFunctionWithoutNoexceptSpecification
    auto swap(Buffer& other) -> void;

private:
    allocator_type m_allocator;
    uint64_t       m_size;
    std::byte*     m_handle;
};

// ReSharper disable once CppSpecialFunctionWithoutNoexceptSpecification
export auto swap(Buffer& lhs, Buffer& rhs) -> void;

}   // namespace kiln::gfx::asset
