#ifndef AETHER_VIEW_H
#define AETHER_VIEW_H

#include <any>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <unordered_map>
#include <vector>

#include <Aether/ADT.h>
#include <Aether/Event.h>
#include <Aether/ViewProperties.h>

namespace xui {

namespace detail {

struct DerefT {
    constexpr decltype(auto) operator()(auto&& p) const {
        return std::forward<decltype(p)>(p).get();
    }
} inline constexpr Get{};

struct PrivateViewKeyT {
    static PrivateViewKeyT const Instance;

private:
    PrivateViewKeyT() = default;
};

constexpr PrivateViewKeyT PrivateViewKeyT::Instance{};
inline constexpr PrivateViewKeyT PrivateViewKey = PrivateViewKeyT::Instance;

static double doubleValOr(double value, double fallback) {
    return std::isnan(value) ? fallback : value;
}

inline double valOrNan(std::optional<double> value) {
    return value.value_or(std::numeric_limits<double>::quiet_NaN());
}

inline namespace viewProperties {

struct MinSize {
    explicit MinSize(Size value = { 0, 0 }): value(value) {}

    Size value;
};

struct MaxSize {
    explicit MaxSize(
        Size value = Size(std::numeric_limits<double>::infinity())):
        value(value) {}

    Size value;
};

struct PrefSize {
    PrefSize(Vec2<std::optional<double>> value = {}):
        value(valOrNan(value.x), valOrNan(value.y)) {}

    Size value;
};

} // namespace viewProperties

} // namespace detail

/// Base class of all views
class View: public WeakRefCountableBase<View> {
public:
    virtual ~View();

    Size minSize() const { return _minSize; }
    double minWidth() const { return _minSize.width(); }
    double minHeight() const { return _minSize.height(); }
    void setMinSize(Size value) { _minSize = value; }
    void setMinWidth(double value) { _minSize.width() = value; }
    void setMinHeight(double value) { _minSize.height() = value; }

    Size maxSize() const { return _maxSize; }
    double maxWidth() const { return _maxSize.width(); }
    double maxHeight() const { return _maxSize.height(); }
    void setMaxSize(Size value) { _maxSize = value; }
    void setMaxWidth(double value) { _maxSize.width() = value; }
    void setMaxHeight(double value) { _maxSize.height() = value; }

    double preferredWidth() const {
        return detail::doubleValOr(_prefSize.width(), minWidth());
    }
    double preferredHeight() const {
        return detail::doubleValOr(_prefSize.height(), minHeight());
    }
    Size preferredSize() const {
        return { preferredWidth(), preferredHeight() };
    }

    void setPreferredWidth(std::optional<double> value) {
        _prefSize.width() = detail::valOrNan(value);
    }
    void setPreferredHeight(std::optional<double> value) {
        _prefSize.height() = detail::valOrNan(value);
    }
    void setPreferredSize(Vec2<std::optional<double>> value) {
        setPreferredWidth(value.x);
        setPreferredHeight(value.y);
    }

    void layout(Rect frame);

    void* nativeHandle() const { return _nativeHandle; }

    Vec2<LayoutMode> layoutMode() const { return _layoutMode; }
    void setLayoutMode(Vec2<LayoutMode> mode) { _layoutMode = mode; }
    void setLayoutModeX(LayoutMode mode) { _layoutMode.x = mode; }
    void setLayoutModeY(LayoutMode mode) { _layoutMode.y = mode; }

    Rect frame() const { return { position(), size() }; }
    Position position() const;
    Size size() const;

    /// \Returns the parent view if this view is attached to another view
    View* parent() { return _parent; }
    View const* parent() const { return _parent; }

    /// \Returns the value of the attribute \p key if present, otherwise
    /// `std::nullopt` A call to this function is ill-format if \p T is not the
    /// type of the attribute \p key
    template <ViewAttributeKey Key>
    std::optional<detail::ViewAttribKeyType<Key>> getAttribute() const {
        if (auto value = getAttributeImpl(Key); value.has_value()) {
            return std::any_cast<detail::ViewAttribKeyType<Key>>(value);
        }
        return std::nullopt;
    }

    /// Sets the attribute \p key to \p value or, if \p value is an empty
    /// `std::optional<T>`, clears the attribute value
    template <ViewAttributeKey Key>
    void setAttribute(std::optional<detail::ViewAttribKeyType<Key>> value) {
        if (value) {
            setAttributeImpl(Key, *std::move(value));
        }
        else {
            clearAttributeImpl(Key);
        }
    }

    template <EventHandlerType... F>
    auto onEvent(F&&... f) {
        ([&] {
            using E = EventHandlerEventType<F>;
            installEventHandler(EventTypeToID<E>, [=](EventUnion const& e) {
                return std::invoke(f, e.get<E>());
            });
        }(), ...);
    }

    void trackMouseMovement(MouseTrackingKind kind,
                            MouseTrackingActivity activity);

    auto subviews() { return _subviews | std::views::transform(detail::Get); }
    auto subviews() const {
        return _subviews | std::views::transform(detail::Get);
    }

    size_t numSubviews() const { return _subviews.size(); }
    View* subviewAt(size_t index) {
        return const_cast<View*>(std::as_const(*this).subviewAt(index));
    }
    View const* subviewAt(size_t index) const {
        assert(index < numSubviews());
        return _subviews[index].get();
    }

    /// Orders this view to the front in its parent view
    void orderFront();

    struct EventImpl;
    struct CustomImpl;

protected:
    View(Vec2<LayoutMode> layoutMode,
         detail::MinSize minSize = detail::MinSize(),
         detail::PrefSize prefSize = detail::PrefSize(),
         detail::MaxSize maxSize = detail::MaxSize());

    View(detail::PrivateViewKeyT, Vec2<LayoutMode> layoutMode,
         detail::MinSize minSize = detail::MinSize(),
         detail::PrefSize prefSize = detail::PrefSize(),
         detail::MaxSize maxSize = detail::MaxSize());

    void setNativeHandle(void* handle);

    virtual bool setFrame(Rect frame);

    void requestLayout();

    View* addSubview(std::unique_ptr<View> view);

    template <std::derived_from<View> V>
    V* addSubview(std::unique_ptr<V> view) {
        return static_cast<V*>(
            addSubview(std::unique_ptr<View>(view.release())));
    }

    void setSubviews(std::vector<std::unique_ptr<View>> views);

    /// Sets the subviews without calling the backing API
    void setSubviewsWeak(detail::PrivateViewKeyT,
                         std::vector<std::unique_ptr<View>> views);

private:
    friend class TabView; // To set _parent

    void installEventHandler(EventType type,
                             std::function<bool(EventUnion const&)> handler);
    void setAttributeImpl(ViewAttributeKey key, std::any value);
    void clearAttributeImpl(ViewAttributeKey key);
    std::any getAttributeImpl(ViewAttributeKey key) const;

    virtual void doLayout(Rect frame) = 0;

    virtual void draw(Rect) {}

    View* _parent = nullptr;
    void* _nativeHandle = nullptr;
    Vec2<LayoutMode> _layoutMode;
    Size _minSize, _maxSize, _prefSize;
    std::vector<std::unique_ptr<View>> _subviews;
    std::unordered_map<ViewAttributeKey, std::any> _attribMap;
    std::unordered_map<EventType, std::function<bool(EventUnion const&)>>
        _eventHandlers;
};

class SpacerView: public View {
public:
    SpacerView();

private:
    void doLayout(Rect) override {}
};

std::unique_ptr<SpacerView> Spacer();

/// Stacks child views along a given axis
class StackView: public View {
public:
    StackView(Axis axis, std::vector<std::unique_ptr<View>> children);

private:
    void doLayout(Rect frame) override;

    Axis axis;
};

/// Horizonal stack view (Axis = X)
///
///     ┌─────────────┬─────────────┬─────────────┐
///     │ children[0] │ children[1] │ children[2] │
///     └─────────────┴─────────────┴─────────────┘
///
std::unique_ptr<StackView> HStack(UniqueVector<View> children);

/// \overload
template <size_t N>
std::unique_ptr<StackView> HStack(std::unique_ptr<View> (&&children)[N]) {
    return HStack(toMoveOnlyVector(std::move(children)));
}

/// Vertical stack view
///
///     ┌─────────────┐
///     │ children[0] │
///     ├─────────────┤
///     │ children[1] │
///     ├─────────────┤
///     │ children[2] │
///     └─────────────┘
///
std::unique_ptr<StackView> VStack(UniqueVector<View> children);

/// \overload
template <size_t N>
std::unique_ptr<StackView> VStack(std::unique_ptr<View> (&&children)[N]) {
    return VStack(toMoveOnlyVector(std::move(children)));
}

/// Views stacked on top of each other
std::unique_ptr<StackView> ZStack(UniqueVector<View> children);

/// \overload
template <size_t N>
std::unique_ptr<StackView> ZStack(std::unique_ptr<View> (&&children)[N]) {
    return ZStack(toMoveOnlyVector(std::move(children)));
}

///
class ScrollView: public View {
public:
    ScrollView(Axis axis, std::vector<std::unique_ptr<View>> children);

private:
    void doLayout(Rect frame) override;
    bool setFrame(Rect frame) override;
    void setDocumentSize(Size size);

    Axis axis;
};

std::unique_ptr<ScrollView> VScrollView(UniqueVector<View> children);

template <size_t N>
std::unique_ptr<ScrollView> VScrollView(std::unique_ptr<View> (&&children)[N]) {
    return VScrollView(toMoveOnlyVector(std::move(children)));
}

std::unique_ptr<ScrollView> HScrollView(UniqueVector<View> children);

template <size_t N>
std::unique_ptr<ScrollView> HScrollView(std::unique_ptr<View> (&&children)[N]) {
    return HScrollView(toMoveOnlyVector(std::move(children)));
}

class SplitView: public View {
public:
    SplitView(Axis axis, std::vector<std::unique_ptr<View>> children);

    SplitterStyle splitterStyle() const { return _splitterStyle; }

    void setSplitterStyle(SplitterStyle style);

    std::optional<Color> const& splitterColor() const { return _splitterColor; }

    void setSplitterColor(std::optional<Color> color);

    std::optional<double> splitterThickness() const {
        return _splitterThickness;
    }

    void setSplitterThickness(std::optional<double> thickness);

    SplitViewResizeStrategy resizeStrategy() const { return _resizeStrategy; }

    void setResizeStrategy(SplitViewResizeStrategy strategy) {
        _resizeStrategy = strategy;
    }

#ifdef AETHER_VIEW_IMPL
public:
#else
private:
#endif
    void didResizeSubviews();
    double constrainSplitPosition(double proposedPosition,
                                  size_t dividerIndex) const;

private:
    void doLayout(Rect frame) override;

    double sizeWithoutDividers() const;
    bool isChildCollapsed(size_t childIndex) const;

    Axis axis;
    SplitterStyle _splitterStyle = SplitterStyle::Default;
    std::optional<Color> _splitterColor;
    std::optional<double> _splitterThickness;
    SplitViewResizeStrategy _resizeStrategy = SplitViewResizeStrategy::Default;
    std::vector<double> childFractions;
};

/// User-resizable horizonal split view (Axis = X)
///
///     ┌─────────────┬─────────────┬─────────────┐
///     │ children[0] │ children[1] │ children[2] │
///     └─────────────┴─────────────┴─────────────┘
///
std::unique_ptr<SplitView> HSplit(UniqueVector<View> children);

/// \overload
template <size_t N>
std::unique_ptr<SplitView> HSplit(std::unique_ptr<View> (&&children)[N]) {
    return HSplit(toMoveOnlyVector(std::move(children)));
}

/// User-resizable vertical split view
///
///     ┌─────────────┐
///     │ children[0] │
///     ├─────────────┤
///     │ children[1] │
///     ├─────────────┤
///     │ children[2] │
///     └─────────────┘
///
std::unique_ptr<SplitView> VSplit(UniqueVector<View> children);

/// \overload
template <size_t N>
std::unique_ptr<SplitView> VSplit(std::unique_ptr<View> (&&children)[N]) {
    return VSplit(toMoveOnlyVector(std::move(children)));
}

enum class TabPosition { None, Top, Left, Bottom, Right };

enum class TabViewBorder { None, Line, Bezel };

struct TabViewElement {
    std::string title;
    std::unique_ptr<View> view;
};

/// TabView
class TabView: public View {
public:
    TabView(std::vector<TabViewElement> elements);

    TabPosition tabPosition() const { return _tabPosition; }

    void setTabPosition(TabPosition position);

    TabViewBorder border() const { return _border; }

    void setBorder(TabViewBorder border);

private:
    void doLayout(Rect frame) override;

    TabPosition _tabPosition = TabPosition::Top;
    TabViewBorder _border = TabViewBorder::Line;

    std::vector<TabViewElement> elements;
};

std::unique_ptr<TabView> Tab(MoveOnlyVector<TabViewElement> elements);

template <size_t N>
std::unique_ptr<TabView> Tab(TabViewElement (&&elements)[N]) {
    return Tab(toMoveOnlyVector(std::move(elements)));
}

enum class ButtonType { Default, Toggle, Switch, Radio };

enum class BezelStyle {
    /// Standard push button style
    Push,
    /// Flexible-height variant of `Push`
    PushFlexHeight,
    /// Circular bezel suitable for a small icon or single character
    Circular,
    ///
    Help,
    /// Squared edges and flexible height
    SmallSquare,
    /// Appropriate for use in a toolbar
    Toolbar,
    ///
    Badge,
};

/// Button
class ButtonView: public View {
public:
    ButtonView(std::string label, std::function<void()> action,
               ButtonType type);

    std::string const& label() const { return _label; }

    std::function<void()> const& action() const { return _action; }

    ButtonType buttonType() const { return _type; }

    BezelStyle bezelStyle() const { return _bezelStyle; }

    void setBezelStyle(BezelStyle style);

    void setLabel(std::string label);

private:
    void doLayout(Rect rect) override;

    ButtonType _type;
    BezelStyle _bezelStyle = BezelStyle::Push;

    std::string _label;
    std::function<void()> _action;
};

/// Push button
///
///     ┌─────────────┐
///     │    Label    │
///     └─────────────┘
///
std::unique_ptr<ButtonView> Button(std::string label,
                                   std::function<void()> action = {});

/// Toggle button that can be toggled between "active" and "inactive" states
///
///     ┌─────────────┐
///     │    Label    │
///     └─────────────┘
///
std::unique_ptr<ButtonView> ToggleButton(std::string label,
                                         std::function<void()> action = {});

/// Switch button that can be toggled between "on" and "off" states
///
///     ⬜︎/⬛︎ Label
///
std::unique_ptr<ButtonView> SwitchButton(std::string label,
                                         std::function<void()> action = {});

/// Radio button that can be toggled between "on" and "off" states
///
///     ◯/◉ Label
///
std::unique_ptr<ButtonView> RadioButton(std::string label,
                                        std::function<void()> action = {});

/// Switch
class SwitchView: public View {
public:
    SwitchView();

private:
    void doLayout(Rect frame) override;
};

///
class TextFieldView: public View {
public:
    explicit TextFieldView(std::string defaultText);

    void setText(std::string text);

    std::string getText() const;

private:
    void doLayout(Rect frame) override;
};

std::unique_ptr<TextFieldView> TextField(std::string defaultText = {});

///
class LabelView: public View {
public:
    explicit LabelView(std::string text);

    void setText(std::string text);

private:
    void doLayout(Rect frame) override;
};

std::unique_ptr<LabelView> Label(std::string text);

///
class ProgressIndicatorView: public View {
public:
    enum Style { Bar, Spinner };

    explicit ProgressIndicatorView(Style style);

private:
    void doLayout(Rect frame) override;
};

std::unique_ptr<ProgressIndicatorView> ProgressBar();

std::unique_ptr<ProgressIndicatorView> ProgressSpinner();

///
class ColorView: public View {
public:
    ColorView(Color const& color);

private:
    void doLayout(Rect frame) override;
};

} // namespace xui

#endif // AETHER_VIEW_H
