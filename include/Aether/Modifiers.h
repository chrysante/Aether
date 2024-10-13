#ifndef AETHER_MODIFIERS_H
#define AETHER_MODIFIERS_H

#include <concepts>
#include <memory>

#include <Aether/ADT.h>
#include <Aether/View.h>

namespace xui {

class View;

template <typename M, typename V>
concept ModifierFor =
    requires(M&& mod, V& view) { applyModifier(std::forward<M>(mod), view); };

template <std::derived_from<View> V>
std::unique_ptr<V> operator|(std::unique_ptr<V>&& view,
                             ModifierFor<V> auto&& mod) {
    applyModifier(std::forward<decltype(mod)>(mod), *view);
    return std::move(view);
}

template <typename V, std::invocable<V&> M>
void applyModifier(M&& mod, V& view) {
    std::invoke(std::forward<M>(mod), view);
}

constexpr auto Flex() {
    return [](View& view) { view.setLayoutMode(LayoutMode::Flex); };
}

constexpr auto XFlex() {
    return [](View& view) {
        view.setLayoutMode({ LayoutMode::Flex, view.layoutMode().y });
    };
}

constexpr auto YFlex() {
    return [](View& view) {
        view.setLayoutMode({ view.layoutMode().x, LayoutMode::Flex });
    };
}

constexpr auto PreferredSize(Size size) {
    return [=](View& view) { view.setPreferredSize(size); };
}

constexpr auto PreferredWidth(double width) {
    return [=](View& view) {
        view.setPreferredSize({ width, view.preferredSize().height() });
    };
}

constexpr auto PreferredHeight(double height) {
    return [=](View& view) {
        view.setPreferredSize({ view.preferredSize().width(), height });
    };
}

constexpr auto MinSize(Size size) {
    return [=](View& view) { view.setMinSize(size); };
}

constexpr auto MinWidth(double width) {
    return [=](View& view) {
        view.setMinSize({ width, view.minSize().height() });
    };
}

constexpr auto MinHeight(double height) {
    return [=](View& view) {
        view.setMinSize({ view.minSize().width(), height });
    };
}

inline void applyModifier(SplitterStyle style, SplitView& view) {
    view.setSplitterStyle(style);
}

inline void applyModifier(SplitViewResizeStrategy strategy, SplitView& view) {
    view.setResizeStrategy(strategy);
}

constexpr auto SplitterColor(std::optional<Color> const& color) {
    return [=](SplitView& view) { view.setSplitterColor(color); };
};

constexpr auto SplitterThickness(std::optional<double> thickness) {
    return [=](SplitView& view) { view.setSplitterThickness(thickness); };
};

namespace detail {

struct SplitViewCollapsableFn {
    explicit constexpr SplitViewCollapsableFn(bool value): value(value) {}

    constexpr SplitViewCollapsableFn operator()(bool value) const {
        return SplitViewCollapsableFn(value);
    }

private:
    friend void applyModifier(SplitViewCollapsableFn obj, View& view) {
        view.setAttribute(detail::ViewAttributeKey::SplitViewCollapsable,
                          obj.value);
    }

    bool value = true;
};

} // namespace detail

inline constexpr detail::SplitViewCollapsableFn SplitViewCollapsable{ true };

/// Modifier used to assign rvalue views to weak references
template <typename V>
auto AssignTo(WeakRef<V>& ref) {
    return [&](V& view) { ref = &view; };
}

} // namespace xui

#endif // AETHER_MODIFIERS_H
