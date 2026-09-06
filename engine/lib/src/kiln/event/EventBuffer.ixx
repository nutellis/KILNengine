module;

#include <algorithm>
#include <memory_resource>
#include <ranges>
#include <type_traits>
#include <utility>
#include <vector>

export module kiln.event.EventBuffer;

import kiln.event.event_c;
import kiln.event.Timestamp;

namespace kiln::event {

export template <event_c Event_T>
class EventBuffer {
public:
    using allocator_type = std::pmr::polymorphic_allocator<>;
    template <bool is_const_T>
    class iterator;
    using const_iterator  = iterator<true>;
    using reference       = std::pair<Event_T&, const Timestamp>;
    using const_reference = std::pair<const Event_T&, const Timestamp>;


    EventBuffer(const EventBuffer&, const allocator_type&)
        requires(std::is_copy_constructible_v<Event_T>);
    EventBuffer(EventBuffer&&, const allocator_type&);

    explicit EventBuffer() = default;
    explicit EventBuffer(std::allocator_arg_t, const allocator_type& allocator);


    [[nodiscard]]
    auto get_allocator() const noexcept -> allocator_type;

    [[nodiscard]]
    auto size() const noexcept -> std::size_t;

    template <typename Self_T>
    [[nodiscard]]
    auto begin(this Self_T& self) noexcept -> iterator<std::is_const_v<Self_T>>;
    [[nodiscard]]
    auto cbegin() const noexcept -> const_iterator;
    template <typename Self_T>
    [[nodiscard]]
    auto end(this Self_T& self) noexcept -> iterator<std::is_const_v<Self_T>>;
    [[nodiscard]]
    auto cend() const noexcept -> const_iterator;


    auto clear() -> void;
    auto reserve(std::size_t capacity) -> void;
    auto insert(Event_T&& event, Timestamp timestamp) -> void;

private:
    using event_container_type     = std::pmr::vector<Event_T>;
    using timestamp_container_type = std::pmr::vector<Timestamp>;


    event_container_type     m_events;
    timestamp_container_type m_timestamps;
};

template <event_c Event_T>
template <bool is_const_T>
class EventBuffer<Event_T>::iterator {
public:
    using iterator_concept  = std::random_access_iterator_tag;
    using iterator_category = std::random_access_iterator_tag;
    using value_type        = std::pair<Event_T, Timestamp>;
    using reference         = std::conditional_t<is_const_T, const_reference, reference>;
    using difference_type   = std::ptrdiff_t;

private:
    template <bool>
    friend class iterator;

    using event_iterator = std::conditional_t<
        is_const_T,
        typename event_container_type::const_iterator,
        typename event_container_type::iterator>;
    using timestamp_iterator = timestamp_container_type::const_iterator;

    struct ArrowProxy {
        reference m_ref;

        auto operator->() -> reference*
        {
            return std::addressof(m_ref);
        }
    };

public:
    iterator() = default;

    explicit(false) iterator(iterator<!is_const_T> other)
        requires is_const_T
        : iterator{ other.m_event_iter, other.m_timestamp_iter }
    {
    }

    iterator(const event_iterator& event_iter, const timestamp_iterator& timestamp_iter)
        : m_event_iter(event_iter),
          m_timestamp_iter(timestamp_iter)
    {
    }

    auto operator==(const iterator other) const noexcept -> bool
    {
        return m_event_iter == other.m_event_iter;
    }

    auto operator*() const -> reference
    {
        return reference{ *m_event_iter, *m_timestamp_iter };
    }

    auto operator->() const -> ArrowProxy
    {
        return ArrowProxy{ **this };
    }

    auto operator++() -> iterator&
    {
        ++m_event_iter;
        ++m_timestamp_iter;
        return *this;
    }

    auto operator++(int) -> iterator
    {
        iterator result{ *this };
        ++m_event_iter;
        ++m_timestamp_iter;
        return result;
    }

    auto operator+=(const difference_type diff) -> iterator&
    {
        m_event_iter += diff;
        m_timestamp_iter += diff;
        return *this;
    }

private:
    event_iterator     m_event_iter;
    timestamp_iterator m_timestamp_iter;
};

}   // namespace kiln::event

namespace kiln::event {

template <event_c Event_T>
EventBuffer<Event_T>::EventBuffer(
    const EventBuffer&    other,
    const allocator_type& allocator
)
    requires(std::is_copy_constructible_v<Event_T>)
    : m_events{ other.m_events, allocator },
      m_timestamps{ other.m_timestamps, allocator }
{
}

template <event_c Event_T>
EventBuffer<Event_T>::EventBuffer(EventBuffer&& other, const allocator_type& allocator)
    : m_events{ std::move(other.m_events), allocator },
      m_timestamps{ std::move(other.m_timestamps), allocator }
{
}

template <event_c Event_T>
EventBuffer<Event_T>::EventBuffer(std::allocator_arg_t, const allocator_type& allocator)
    : m_events(allocator),
      m_timestamps{ allocator }
{
}

template <event_c Event_T>
auto EventBuffer<Event_T>::get_allocator() const noexcept -> allocator_type
{
    return m_events.get_allocator();
}

template <event_c Event_T>
auto EventBuffer<Event_T>::size() const noexcept -> std::size_t
{
    return m_events.size();
}

template <event_c Event_T>
template <typename Self_T>
auto EventBuffer<Event_T>::begin(this Self_T& self) noexcept
    -> iterator<std::is_const_v<Self_T>>
{
    return iterator<std::is_const_v<Self_T>>{
        self.template EventBuffer<Event_T>::m_events.begin(),
        self.template EventBuffer<Event_T>::m_timestamps.begin(),
    };
}

template <event_c Event_T>
auto EventBuffer<Event_T>::cbegin() const noexcept -> const_iterator
{
    return const_iterator{
        m_events.cbegin(),
        m_timestamps.cbegin(),
    };
}

template <event_c Event_T>
template <typename Self_T>
auto EventBuffer<Event_T>::end(this Self_T& self) noexcept
    -> iterator<std::is_const_v<Self_T>>
{
    return iterator<std::is_const_v<Self_T>>{
        self.template EventBuffer<Event_T>::m_events.end(),
        self.template EventBuffer<Event_T>::m_timestamps.end(),
    };
}

template <event_c Event_T>
auto EventBuffer<Event_T>::cend() const noexcept -> const_iterator
{
    return const_iterator{
        m_events.cend(),
        m_timestamps.cend(),
    };
}

template <event_c Event_T>
auto EventBuffer<Event_T>::clear() -> void
{
    m_events.clear();
    m_timestamps.clear();
}

template <event_c Event_T>
auto EventBuffer<Event_T>::reserve(const std::size_t capacity) -> void
{
    m_events.reserve(capacity);
    m_timestamps.reserve(capacity);
}

template <event_c Event_T>
auto EventBuffer<Event_T>::insert(Event_T&& event, const Timestamp timestamp) -> void
{
    const auto timestamp_iter{ std::ranges::lower_bound(m_timestamps, timestamp) };
    const auto event_iter{
        std::next(m_events.begin(), std::distance(m_timestamps.begin(), timestamp_iter))
    };

    m_timestamps.insert(timestamp_iter, timestamp);
    m_events.insert(event_iter, std::move(event));
}

}   // namespace kiln::event
