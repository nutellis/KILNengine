module;

#include <memory_resource>

export module kiln.util.memory.Deleter;

namespace kiln::util {

export class Deleter {
    using Allocator = std::pmr::polymorphic_allocator<>;

public:
    constexpr explicit Deleter(const Allocator& allocator) : m_allocator{ allocator } {}

    template <typename T>
    constexpr auto operator()(T* pointer) -> void
    {
        m_allocator.delete_object(pointer);
    }

    [[nodiscard]]
    constexpr auto allocator() const noexcept -> Allocator
    {
        return m_allocator;
    }

private:
    Allocator m_allocator;
};

}   // namespace kiln::util
