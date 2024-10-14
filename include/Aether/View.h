#ifndef AETHER_VIEW_H
#define AETHER_VIEW_H

#include <any>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <Aether/ADT.h>
#include <Aether/ViewProperties.h>

namespace xui {

/// Base class of all views
class View: public WeakRefCountableBase<View> {
public:
    virtual ~View();

    void layout(Rect frame);

    void* nativeHandle() const { return _nativeHandle; }

    Size minSize() const { return _minSize; }
    Size maxSize() const { return _maxSize; }
    Size preferredSize() const { return _preferredSize; }
    Vec2<LayoutMode> layoutMode() const { return _layoutMode; }

    void setMinSize(Size size) { _minSize = size; }
    void setMaxSize(Size size) { _maxSize = size; }
    void setPreferredSize(Size size) { _preferredSize = size; }
    void setLayoutMode(Vec2<LayoutMode> mode) { _layoutMode = mode; }

    Rect frame() const { return { position(), size() }; }
    Position position() const;
    Size size() const;

    View* parent() { return _parent; }
    View const* parent() const { return _parent; }

    /// \Returns the value of the attribute \p key if present, otherwise
    /// `std::nullopt` A call to this function is ill-format if \p T is not the
    /// type of the attribute \p key
    template <typename T>
    std::optional<T> getAttribute(detail::ViewAttributeKeyGetter<T> key) const;

    /// Sets the attribute \p key to \p value or, if \p value is an empty
    /// `std::optional<T>`, clears the attribute value
    template <typename T>
    void setAttribute(detail::ViewAttributeKeySetter<T> key, T&& value);

protected:
    void setNativeHandle(void* handle);

    Size _minSize{}, _maxSize{ INFINITY, INFINITY }, _preferredSize{};
    Vec2<LayoutMode> _layoutMode = LayoutMode::Static;

private:
    friend class AggregateView; // To set _parent

    void setAttributeImpl(ViewAttributeKey key, std::any value);
    void clearAttributeImpl(ViewAttributeKey key);
    std::any getAttributeImpl(ViewAttributeKey key) const;

    virtual void doLayout(Rect rect) = 0;

    View* _parent = nullptr;
    void* _nativeHandle = nullptr;
    std::unordered_map<ViewAttributeKey, std::any> _attribMap;
};

/// Inline implementation

template <typename T>
std::optional<T> View::getAttribute(
    detail::ViewAttributeKeyGetter<T> key) const {
    if (auto value = getAttributeImpl(key.value); value.has_value()) {
        return std::any_cast<T>(value);
    }
    return std::nullopt;
}

template <typename T>
void View::setAttribute(detail::ViewAttributeKeySetter<T> key, T&& value) {
    switch (key.value) {
#define X(Name, Type)                                                          \
    case ViewAttributeKey::Name:                                               \
        if (std::optional<detail::ViewAttribKeyType<ViewAttributeKey::Name>>   \
                optVal = std::forward<T>(value))                               \
        {                                                                      \
            setAttributeImpl(key.value, *std::move(optVal));                   \
        }                                                                      \
        else {                                                                 \
            clearAttributeImpl(key.value);                                     \
        }                                                                      \
        break;
        AETHER_VIEW_ATTRIB_KEY(X)
#undef X
    }
}

class SpacerView: public View {
public:
    SpacerView();

private:
    void doLayout(Rect) override {}
};

std::unique_ptr<SpacerView> Spacer();

class AggregateView: public View {
public:
    Axis axis() const { return _axis; }

protected:
    AggregateView(Axis axis, std::vector<std::unique_ptr<View>> children);

private:
    Axis _axis;

protected:
    std::vector<std::unique_ptr<View>> _children;
};

/// Stacks child views along a given axis
class StackView: public AggregateView {
public:
    StackView(Axis axis, std::vector<std::unique_ptr<View>> children);

private:
    void doLayout(Rect frame) override;
    void setFrame(Rect frame);
};

std::unique_ptr<StackView> HStack(UniqueVector<View> children);

template <size_t N>
std::unique_ptr<StackView> HStack(std::unique_ptr<View> (&&children)[N]) {
    return HStack(toUniqueVector(std::move(children)));
}

std::unique_ptr<StackView> VStack(UniqueVector<View> children);

template <size_t N>
std::unique_ptr<StackView> VStack(std::unique_ptr<View> (&&children)[N]) {
    return VStack(toUniqueVector(std::move(children)));
}

///
class ScrollView: public AggregateView {
public:
    ScrollView(Axis axis, std::vector<std::unique_ptr<View>> children);

private:
    void doLayout(Rect frame) override;
    void setFrame(Rect frame);
    void setDocumentSize(Size size);
};

std::unique_ptr<ScrollView> VScrollView(UniqueVector<View> children);

template <size_t N>
std::unique_ptr<ScrollView> VScrollView(std::unique_ptr<View> (&&children)[N]) {
    return VScrollView(toUniqueVector(std::move(children)));
}

std::unique_ptr<ScrollView> HScrollView(UniqueVector<View> children);

template <size_t N>
std::unique_ptr<ScrollView> HScrollView(std::unique_ptr<View> (&&children)[N]) {
    return HScrollView(toUniqueVector(std::move(children)));
}

class SplitView: public AggregateView {
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
    return HSplit(toUniqueVector(std::move(children)));
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
    return VSplit(toUniqueVector(std::move(children)));
}

/// Button
class ButtonView: public View {
public:
    ButtonView(std::string label, std::function<void()> action);

    std::string const& label() const { return _label; }

    std::function<void()> const& action() const { return _action; }

private:
    void doLayout(Rect rect) override;

    std::string _label;
    std::function<void()> _action;
};

std::unique_ptr<ButtonView> Button(std::string label,
                                   std::function<void()> action = {});

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
