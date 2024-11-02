#ifndef AETHER_MODIFIERS_H
#define AETHER_MODIFIERS_H

#include <concepts>
#include <memory>

#include <Aether/ADT.h>
#include <Aether/Vec.h>
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

template <typename M, typename V>
concept StrongModifierFor = requires(M&& mod, std::unique_ptr<V> view) {
    {
        applyModifier(std::forward<M>(mod), std::move(view))
    } -> std::convertible_to<std::unique_ptr<View>>;
};

template <std::derived_from<View> V>
auto operator|(std::unique_ptr<V>&& view, StrongModifierFor<V> auto&& mod) {
    return applyModifier(std::forward<decltype(mod)>(mod), std::move(view));
}

template <typename V, std::invocable<V&> M>
void applyModifier(M&& mod, V& view) {
    std::invoke(std::forward<M>(mod), view);
}

constexpr auto Flex() {
    return [](View& view) {
        view.setLayoutMode({ LayoutMode::Flex, LayoutMode::Flex });
    };
}

constexpr auto XFlex() {
    return [](View& view) { view.setLayoutModeX(LayoutMode::Flex); };
}

constexpr auto YFlex() {
    return [](View& view) { view.setLayoutModeY(LayoutMode::Flex); };
}

constexpr auto Static() {
    return [](View& view) {
        view.setLayoutMode({ LayoutMode::Static, LayoutMode::Static });
    };
}

constexpr auto XStatic() {
    return [](View& view) { view.setLayoutModeX(LayoutMode::Static); };
}

constexpr auto YStatic() {
    return [](View& view) { view.setLayoutModeY(LayoutMode::Static); };
}

constexpr auto PreferredSize(Size size) {
    return [=](View& view) {
        view.setPreferredSize({ size.width(), size.height() });
    };
}

constexpr auto PreferredWidth(double width) {
    return [=](View& view) { view.setPreferredWidth(width); };
}

constexpr auto PreferredHeight(double height) {
    return [=](View& view) { view.setPreferredHeight(height); };
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

constexpr auto PaddingX(std::optional<double> value) {
    return [=](View& view) {
        view.setAttribute<ViewAttributeKey::PaddingX>(value);
    };
};

constexpr auto PaddingY(std::optional<double> value) {
    return [=](View& view) {
        view.setAttribute<ViewAttributeKey::PaddingY>(value);
    };
};

inline void applyModifier(AlignX align, View& view) {
    view.setAttribute<ViewAttributeKey::AlignX>(align);
}

inline void applyModifier(AlignY align, View& view) {
    view.setAttribute<ViewAttributeKey::AlignY>(align);
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

template <ViewAttributeKey Key, typename V = View>
struct AttribKeyFn {
    using ValueType = detail::ViewAttribKeyType<Key>;
    using ViewType = V;

    explicit constexpr AttribKeyFn(ValueType value): value(value) {}

    constexpr AttribKeyFn operator()(ValueType value) const {
        return AttribKeyFn(std::move(value));
    }

private:
    friend void applyModifier(AttribKeyFn obj, ViewType& view) {
        view.template setAttribute<Key>(obj.value);
    }

    bool value = true;
};

} // namespace detail

inline constexpr detail::AttribKeyFn<ViewAttributeKey::SplitViewCollapsable>
    SplitViewCollapsable{ true };
inline constexpr detail::AttribKeyFn<ViewAttributeKey::SplitViewResizeWeight>
    SplitViewResizeWeight{ true };

inline void applyModifier(TabPosition pos, TabView& view) {
    view.setTabPosition(pos);
}

inline void applyModifier(TabViewBorder border, TabView& view) {
    view.setBorder(border);
}

inline void applyModifier(BezelStyle style, ButtonView& button) {
    button.setBezelStyle(style);
}

/// Modifier used to assign rvalue views to weak references
template <typename V>
constexpr auto AssignTo(WeakRef<V>& ref) {
    return [&](V& view) { ref = &view; };
}

/// Installs event handlers to a view
template <EventHandlerType... F>
auto OnEvent(F&&... f) {
    return [=](View& view) { view.addEventHandler(f...); };
}

/// Enables mouse tracking of a view
inline auto TrackMouseMovement(MouseTrackingKind kind,
                               MouseTrackingActivity activity) {
    return [=](View& view) { view.trackMouseMovement(kind, activity); };
}

template <std::derived_from<View> U, std::derived_from<View> V,
          std::derived_from<V> W>
inline std::unique_ptr<U> applyModifier(
    std::unique_ptr<U> (*mod)(std::unique_ptr<V>), std::unique_ptr<W> view) {
    return mod(std::move(view));
}

} // namespace xui

#endif // AETHER_MODIFIERS_H
