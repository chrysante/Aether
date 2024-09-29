#ifndef AETHER_VIEW_H
#define AETHER_VIEW_H

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <Aether/ADT.h>

namespace xui {

/// Base class of all views
class View: public WeakRefCountableBase<View> {
public:
    virtual ~View();

    void layout(Rect rect) { doLayout(rect); }

    void* nativeHandle() const { return _handle; }

    Size minSize() const { return _minSize; }
    Size maxSize() const { return _maxSize; }
    Size preferredSize() const { return _preferredSize; }
    Vec2<LayoutMode> layoutMode() const { return _layoutMode; }

    void setMinSize(Size size) { _minSize = size; }
    void setMaxSize(Size size) { _maxSize = size; }
    void setPreferredSize(Size size) { _preferredSize = size; }
    void setLayoutMode(Vec2<LayoutMode> mode) { _layoutMode = mode; }

protected:
    void* _handle = nullptr;

    Size _minSize{}, _maxSize{ INFINITY, INFINITY }, _preferredSize{};
    Vec2<LayoutMode> _layoutMode = LayoutMode::Static;

private:
    virtual void doLayout(Rect rect) = 0;
};

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
    AggregateView(Axis axis, std::vector<std::unique_ptr<View>> children):
        _axis(axis), _children(std::move(children)) {}

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

} // namespace xui

#endif // AETHER_VIEW_H
