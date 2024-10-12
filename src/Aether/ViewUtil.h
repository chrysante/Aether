#ifndef AETHER_VIEWUTIL_H
#define AETHER_VIEWUTIL_H

#include <type_traits>

#include "Aether/ADT.h"

namespace xui {

static decltype(auto) dispatchAxis(Axis axis, auto&& f) {
    switch (axis) {
    case Axis::X:
        return std::invoke(f, std::integral_constant<Axis, Axis::X>{});
    case Axis::Y:
        return std::invoke(f, std::integral_constant<Axis, Axis::Y>{});
    case Axis::Z:
        return std::invoke(f, std::integral_constant<Axis, Axis::Z>{});
    }
}

} // namespace xui

#endif // AETHER_VIEWUTIL_H
