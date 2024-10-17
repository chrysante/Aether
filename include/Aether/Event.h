#ifndef AETHER_EVENT_H
#define AETHER_EVENT_H

namespace xui {

enum class EventType {
#define AETHER_EVENT_TYPE_DEF(Name) Name,
#include <Aether/Event.def>
};

class Event {
public:
    EventType type() const { return _type; }

protected:
    explicit Event(EventType type): _type(type) {}

private:
    EventType _type;
};

class MouseEvent: public Event {
protected:
    explicit MouseEvent(EventType type): Event(type) {}
};

class ScrollEvent: public MouseEvent {
public:
    ScrollEvent(): MouseEvent(EventType::ScrollEvent) {}
};

class EventUnion {
public:
#define AETHER_CONCRETE_EVENT_TYPE_DEF(Name)                                   \
    EventUnion(Name value): m_##Name(std::move(value)) {}
#include <Aether/Event.def>

    EventType type() const { return m_ScrollEvent.type(); }

    template <typename E>
    auto const& get() const {
        if constexpr (false) {
        }
#define AETHER_CONCRETE_EVENT_TYPE_DEF(Name)                                   \
    else if constexpr (std::is_same_v<E, xui::Name>) {                         \
        assert(type() == EventType::Name);                                     \
        return m_##Name;                                                       \
    }
#include <Aether/Event.def>
        else {
            static_assert(!std::is_same_v<E, E>, "Invalid type");
        }
    }

private:
    union {
#define AETHER_CONCRETE_EVENT_TYPE_DEF(Name) Name m_##Name;
#include <Aether/Event.def>
    };
};

template <typename>
inline constexpr auto EventTypeToID = nullptr;

#define AETHER_EVENT_TYPE_DEF(Name)                                            \
    template <>                                                                \
    inline constexpr EventType EventTypeToID<Name> = EventType::Name;
#include <Aether/Event.def>

template <typename F,
          typename StdFunction = decltype(std::function{ std::declval<F>() })>
struct EventHandlerEventTypeImpl;

template <typename F, typename R, typename E>
struct EventHandlerEventTypeImpl<F, std::function<R(E)>>:
    std::remove_cvref<E> {};

template <typename F>
using EventHandlerEventType = typename EventHandlerEventTypeImpl<F>::type;

} // namespace xui

#endif // AETHER_EVENT_H
