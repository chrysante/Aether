#ifndef AETHER_EVENT_H
#define AETHER_EVENT_H

#include <functional>
#include <type_traits>

#include <csp.hpp>

#include <Aether/Vec.h>

namespace xui {

class Window;

enum class MouseButton { Left, Right, Other };

/// Identifiers for each event type
enum class EventType {
#define AETHER_EVENT_TYPE_DEF(Name, ...) Name,
#include <Aether/Event.def>
};

/// Forward declarations of all event types
#define AETHER_EVENT_TYPE_DEF(Name, ...) class Name;
#include <Aether/Event.def>

using NoParent = void;

} // namespace xui

#define AETHER_EVENT_TYPE_DEF(Name, Parent, Corporeality)                      \
    CSP_DEFINE(xui::Name, xui::EventType::Name, xui::Parent, Corporeality)
#include <Aether/Event.def>

namespace xui {

/// Base class of all events
class Event: public csp::base_helper<Event> {
public:
    EventType type() const { return get_rtti(*this); }

    Window* window() const { return _window; }

protected:
    explicit Event(EventType type, Window* window):
        base_helper(type), _window(window) {}

private:
    Window* _window;
};

/// Base class of all mouse events
class MouseEvent: public Event {
public:
    Vec2<double> locationInWindow() const { return _locationInWindow; }

protected:
    explicit MouseEvent(EventType type, Window* window,
                        Vec2<double> locationInWindow):
        Event(type, window), _locationInWindow(locationInWindow) {}

private:
    Vec2<double> _locationInWindow;
};

/// Base class of `MouseDownEvent` and `MouseUpEvent`
class MouseClickEvent: public MouseEvent {
public:
    MouseButton mouseButton() const { return _button; }

protected:
    explicit MouseClickEvent(EventType type, MouseButton button, Window* window,
                             Vec2<double> locationInWindow):
        MouseEvent(type, window, locationInWindow), _button(button) {}

private:
    MouseButton _button;
};

/// Base class of `MouseMoveEvent` and `MouseDragEvent`
class MouseMotionEvent: public MouseEvent {
public:
    Vec2<double> delta() const { return _delta; }

protected:
    explicit MouseMotionEvent(EventType type, Window* window,
                              Vec2<double> locationInWindow,
                              Vec2<double> delta):
        MouseEvent(type, window, locationInWindow), _delta(delta) {}

private:
    Vec2<double> _delta;
};

/// Base class of `MouseEnterEvent` and `MouseExitEvent`
class MouseTransitionEvent: public MouseEvent {
protected:
    explicit MouseTransitionEvent(EventType type, Window* window,
                                  Vec2<double> locationInWindow):
        MouseEvent(type, window, locationInWindow) {}
};

/// Sent when the mouse is clicked down
class MouseDownEvent: public MouseClickEvent {
public:
    explicit MouseDownEvent(MouseButton button, Window* window,
                            Vec2<double> locationInWindow):
        MouseClickEvent(EventType::MouseDownEvent, button, window,
                        locationInWindow) {}
};

/// Sent when the mouse is released
class MouseUpEvent: public MouseClickEvent {
public:
    explicit MouseUpEvent(MouseButton button, Window* window,
                          Vec2<double> locationInWindow):
        MouseClickEvent(EventType::MouseUpEvent, button, window,
                        locationInWindow) {}
};

/// Sent when the mouse is moved. Requires the view or window to explicitly opt
/// in to mouse tracking
class MouseMoveEvent: public MouseMotionEvent {
public:
    explicit MouseMoveEvent(Window* window, Vec2<double> locationInWindow,
                            Vec2<double> delta):
        MouseMotionEvent(EventType::MouseMoveEvent, window, locationInWindow,
                         delta) {}
};

/// Sent when the mouse is dragged
class MouseDragEvent: public MouseMotionEvent {
public:
    explicit MouseDragEvent(MouseButton button, Window* window,
                            Vec2<double> locationInWindow, Vec2<double> delta):
        MouseMotionEvent(EventType::MouseDragEvent, window, locationInWindow,
                         delta),
        _button(button) {}

    MouseButton mouseButton() const { return _button; }

private:
    MouseButton _button;
};

/// Sent when the mouse enters a view or window. Requires the view or window to
/// explicitly opt in to mouse tracking
class MouseEnterEvent: public MouseTransitionEvent {
public:
    explicit MouseEnterEvent(Window* window, Vec2<double> locationInWindow):
        MouseTransitionEvent(EventType ::MouseEnterEvent, window,
                             locationInWindow) {}
};

/// Sent when the mouse exits a view or window. Requires the view or window to
/// explicitly opt in to mouse tracking
class MouseExitEvent: public MouseTransitionEvent {
public:
    explicit MouseExitEvent(Window* window, Vec2<double> locationInWindow):
        MouseTransitionEvent(EventType::MouseExitEvent, window,
                             locationInWindow) {}
};

enum class MomentumPhase {
    None,
    Began,
    Stationary,
    Changed,
    Ended,
    Cancelled,
    MayBegin,
};

/// Sent when the mouse is scrolled
class ScrollEvent: public MouseEvent {
public:
    explicit ScrollEvent(Window* window, Vec2<double> locationInWindow,
                         Vec2<double> delta, MomentumPhase phase):
        MouseEvent(EventType::ScrollEvent, window, locationInWindow),
        _delta(delta),
        _momentumPhase(phase) {}

    Vec2<double> delta() const { return _delta; }

    MomentumPhase momentumPhase() const { return _momentumPhase; }

private:
    Vec2<double> _delta;
    MomentumPhase _momentumPhase;
};

using EventUnion = csp::dyn_union<Event>;

template <typename>
inline constexpr auto EventTypeToID = nullptr;

#define AETHER_EVENT_TYPE_DEF(Name, ...)                                       \
    template <>                                                                \
    inline constexpr EventType EventTypeToID<Name> = EventType::Name;
#include <Aether/Event.def>

template <typename F, typename = decltype(std::function{ std::declval<F>() })>
struct EventHandlerEventTypeImpl;

template <typename F, typename R, typename E>
struct EventHandlerEventTypeImpl<F, std::function<R(E)>>:
    std::remove_cvref<E> {};

template <typename F>
using EventHandlerEventType = typename EventHandlerEventTypeImpl<F>::type;

template <typename F>
concept EventHandlerType = std::predicate<F, EventHandlerEventType<F>>;

} // namespace xui

#endif // AETHER_EVENT_H
