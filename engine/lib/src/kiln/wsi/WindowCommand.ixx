module;

#include <memory_resource>
#include <utility>

export module kiln.wsi.WindowCommand;

import kiln.util.containers.CopyableFunction;
import kiln.wsi.Context;

namespace kiln::wsi {

export class WindowCommand {
    using Command = util::CopyableFunction<auto(const Context&) &&->void>;

public:
    using allocator_type = std::pmr::polymorphic_allocator<>;


    WindowCommand(const WindowCommand&, const allocator_type&);
    WindowCommand(WindowCommand&&, const allocator_type&);

    template <typename F>
    explicit WindowCommand(F&& command)
        requires(!std::is_same_v<std::remove_cvref_t<F>, WindowCommand>)
             && std::constructible_from<Command, F&&>;
    template <typename F>
    explicit WindowCommand(
        std::allocator_arg_t,
        const allocator_type& allocator,
        F&&                   command
    )
        requires(!std::is_same_v<std::remove_cvref_t<F>, WindowCommand>)
             && std::constructible_from<Command, F&&>;


    auto operator()(const Context& context) && -> void;

    [[nodiscard]]
    auto get_allocator() const noexcept -> allocator_type;


private:
    Command m_command;
};

}   // namespace kiln::wsi

namespace kiln::wsi {

WindowCommand::WindowCommand(const WindowCommand& other, const allocator_type& allocator)
    : m_command{ other.m_command, allocator }
{
}

WindowCommand::WindowCommand(WindowCommand&& other, const allocator_type& allocator)
    : m_command{ std::move(other.m_command), allocator }
{
}

template <typename F>
WindowCommand::WindowCommand(F&& command)
    requires(!std::is_same_v<std::remove_cvref_t<F>, WindowCommand>)
         && std::constructible_from<Command, F&&>
    : m_command{ std::forward<F>(command) }
{
}

template <typename F>
WindowCommand::WindowCommand(
    std::allocator_arg_t,
    const allocator_type& allocator,
    F&&                   command
)
    requires(!std::is_same_v<std::remove_cvref_t<F>, WindowCommand>)
         && std::constructible_from<Command, F&&>
    : m_command{ std::allocator_arg, allocator, std::forward<F>(command) }
{
}

auto WindowCommand::operator()(const Context& context) && -> void
{
    std::move(m_command).operator()(context);
}

auto WindowCommand::get_allocator() const noexcept -> allocator_type
{
    return m_command.get_allocator();
}

}   // namespace kiln::wsi
