module;

#include <functional>

#include "kiln/util/contract_macros.hpp"
#include "kiln/util/memory/lifetimebound.hpp"

export module kiln.gfx.renderer.memory.BufferRegion;

import vulkan;

import kiln.gfx.renderer.memory.Allocation;
import kiln.gfx.renderer.memory.Buffer;
import kiln.util.contracts;

namespace kiln::gfx::renderer {

class BufferRegionPrecondition {
public:
    explicit BufferRegionPrecondition() = default;

    explicit BufferRegionPrecondition(
        [[maybe_unused]] const Buffer&        buffer,
        [[maybe_unused]] const vk::DeviceSize offset
    )
    {
        PRECOND(buffer.size() >= offset);
    }

    explicit BufferRegionPrecondition(
        [[maybe_unused]] const Buffer&        buffer,
        [[maybe_unused]] const vk::DeviceSize offset,
        [[maybe_unused]] const vk::DeviceSize size
    )
        : BufferRegionPrecondition{ buffer, offset }
    {
        PRECOND(buffer.size() - offset >= size);
    }
};

export class BufferRegion : BufferRegionPrecondition {
public:
    explicit(false) BufferRegion(kiln_lifetimebound Buffer& buffer)
        : m_buffer{ buffer },
          m_offset{ 0 },
          m_size{ buffer.size() }
    {
    }

    explicit BufferRegion(kiln_lifetimebound Buffer& buffer, const vk::DeviceSize offset)
        : BufferRegionPrecondition{ buffer, offset },
          m_buffer{ buffer },
          m_offset{ offset },
          m_size{ buffer.size() - offset }
    {
    }

    explicit BufferRegion(
        kiln_lifetimebound Buffer& buffer,
        const vk::DeviceSize       offset,
        const vk::DeviceSize       size
    )
        : BufferRegionPrecondition{ buffer, offset, size },
          m_buffer{ buffer },
          m_offset{ offset },
          m_size{ size }
    {
    }

    [[nodiscard]]
    auto buffer() const noexcept -> Buffer&
    {
        return m_buffer.get();
    }

    [[nodiscard]]
    auto allocation() const noexcept -> Allocation&
    {
        return m_buffer.get().allocation();
    }

    [[nodiscard]]
    auto offset() const noexcept -> vk::DeviceSize
    {
        return m_offset;
    }

    [[nodiscard]]
    auto size() const noexcept -> vk::DeviceSize
    {
        return m_size;
    }

private:
    std::reference_wrapper<Buffer> m_buffer;
    vk::DeviceSize                 m_offset;
    vk::DeviceSize                 m_size;
};

}   // namespace kiln::gfx::renderer
