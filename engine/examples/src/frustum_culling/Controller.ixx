module;

#include <glm/ext/quaternion_double.hpp>
#include <glm/ext/vector_double2.hpp>
#include <glm/ext/vector_double3.hpp>

export module examples.frustum_culling.Controller;

import kiln.event.Timestamp;
import kiln.wsi.event.Event;
import kiln.wsi.event.events;
import kiln.wsi.event.Key;
import kiln.wsi.Position;
import kiln.wsi.WindowProxy;

import examples.frustum_culling.Camera;

namespace demo {

export class Controller {
public:
    explicit Controller(
        const kiln::wsi::WindowProxy& window,
        double                        movement_speed = 1
    ) noexcept;


    auto update(Camera& camera) const noexcept -> void;

    auto update(kiln::event::Timestamp timestamp) noexcept -> void;
    auto update(
        const kiln::wsi::Event& event,
        kiln::event::Timestamp  timestamp,
        kiln::wsi::WindowProxy& window
    ) noexcept -> void;

private:
    struct MovementKeys {
        kiln::wsi::Key forward{ kiln::wsi::Key::eW };
        kiln::wsi::Key backward{ kiln::wsi::Key::eS };
        kiln::wsi::Key left{ kiln::wsi::Key::eA };
        kiln::wsi::Key right{ kiln::wsi::Key::eD };
        kiln::wsi::Key up{ kiln::wsi::Key::eE };
        kiln::wsi::Key down{ kiln::wsi::Key::eQ };
    };

    struct MovementStates {
        bool forward{};
        bool backward{};
        bool left{};
        bool right{};
        bool up{};
        bool down{};
    };

    constexpr static MovementKeys movement_keys{
        .forward  = kiln::wsi::Key::eW,
        .backward = kiln::wsi::Key::eS,
        .left     = kiln::wsi::Key::eA,
        .right    = kiln::wsi::Key::eD,
        .up       = kiln::wsi::Key::eE,
        .down     = kiln::wsi::Key::eQ,
    };
    constexpr static double movement_sensitivity{ 0.01 };
    constexpr static double rotation_sensitivity{ 0.002 };

    bool                   m_cursor_disabled{};
    kiln::event::Timestamp m_time;
    MovementStates         m_movement_states;
    glm::dvec3             m_position{};
    glm::dvec2             m_last_cursor_position;
    double                 m_pitch{};
    double                 m_yaw{};
    double                 m_movement_speed;


    [[nodiscard]]
    auto movement_direction() const noexcept -> glm::dvec3;
    [[nodiscard]]
    auto movement_orientation() const noexcept -> glm::dquat;
    [[nodiscard]]
    auto orientation() const noexcept -> glm::dquat;

    auto update(
        const kiln::wsi::KeyPressedEvent& key_pressed_event,
        kiln::event::Timestamp            timestamp,
        kiln::wsi::WindowProxy&           window
    ) noexcept -> void;
    auto update(
        const kiln::wsi::KeyReleasedEvent& key_released_event,
        kiln::event::Timestamp             timestamp,
        kiln::wsi::WindowProxy&            window
    ) noexcept -> void;
    auto update(
        const kiln::wsi::CursorMovedEvent& cursor_moved_event,
        kiln::event::Timestamp             timestamp
    ) noexcept -> void;
};

}   // namespace demo
