#ifndef AETHER_MODIFIERS_H
#define AETHER_MODIFIERS_H

#include <concepts>
#include <memory>

#include <Aether/ADT.h>

namespace xui {

class View;

template <typename M, typename V>
concept ModifierFor = std::invocable<M, V&>;

template <std::derived_from<View> V>
std::unique_ptr<V> operator|(std::unique_ptr<V>&& view,
                             ModifierFor<V> auto const& mod) {
    std::invoke(mod, *view);
    return std::move(view);
}

constexpr auto Flex() {
    return [](auto& view) { view.setLayoutMode(LayoutMode::Flex); };
}

constexpr auto XFlex() {
    return [](auto& view) {
        view.setLayoutMode({ LayoutMode::Flex, view.layoutMode().y });
    };
}

constexpr auto YFlex() {
    return [](auto& view) {
        view.setLayoutMode({ view.layoutMode().x, LayoutMode::Flex });
    };
}

constexpr auto PreferredSize(Size size) {
    return [=](auto& view) { view.setPreferredSize(size); };
}

constexpr auto PreferredWidth(double width) {
    return [=](auto& view) {
        view.setPreferredSize({ width, view.preferredSize().height() });
    };
}

constexpr auto PreferredHeight(double height) {
    return [=](auto& view) {
        view.setPreferredSize({ view.preferredSize().width(), height });
    };
}

} // namespace xui

#endif // AETHER_MODIFIERS_H
