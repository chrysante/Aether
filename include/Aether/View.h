#ifndef AETHER_VIEW_H
#define AETHER_VIEW_H

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <Aether/ADT.h>

namespace xui {

enum class LayoutMode { Static, Flex };

/// Base class of all views
class View {
public:
    virtual ~View();

    void layout(Rect rect) { doLayout(rect); }

    void* nativeHandle() const { return _handle; }

    Size minSize() const { return _minSize; }
    Size maxSize() const { return _maxSize; }
    Size preferredSize() const { return _preferredSize; }
    LayoutMode layoutMode() const { return _layoutMode; }

protected:
    void* _handle = nullptr;

    Size _minSize{}, _maxSize{ INFINITY, INFINITY }, _preferredSize{};
    LayoutMode _layoutMode = LayoutMode::Static;

private:
    virtual void doLayout(Rect rect) = 0;
};

class Spacer: public View {
public:
    Spacer();

private:
    void doLayout(Rect) override {}
};

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
    void layoutChildren(Rect frame);
    void setFrame(Rect frame);
};

std::unique_ptr<StackView> hStack(UniqueVector<View> children);

template <size_t N>
std::unique_ptr<StackView> hStack(std::unique_ptr<View> (&&children)[N]) {
    return hStack(toUniqueVector(std::move(children)));
}

std::unique_ptr<StackView> vStack(UniqueVector<View> children);

template <size_t N>
std::unique_ptr<StackView> vStack(std::unique_ptr<View> (&&children)[N]) {
    return vStack(toUniqueVector(std::move(children)));
}

///
class ScrollView: public AggregateView {
public:
    ScrollView(Axis axis, std::vector<std::unique_ptr<View>> children);

private:
    void doLayout(Rect frame) override;
    void setFrame(Rect frame);
};

std::unique_ptr<ScrollView> scrollView(UniqueVector<View> children);

template <size_t N>
std::unique_ptr<ScrollView> scrollView(std::unique_ptr<View> (&&children)[N]) {
    return scrollView(toUniqueVector(std::move(children)));
}

/// Button
class Button: public View {
public:
    Button(std::string label, std::function<void()> action);

    std::string const& label() const { return _label; }

    std::function<void()> const& action() const { return _action; }

private:
    void doLayout(Rect rect) override;

    std::string _label;
    std::function<void()> _action;
};

std::unique_ptr<Button> button(std::string label, std::function<void()> action);

///
class TextField: public View {
public:
    explicit TextField(std::string defaultText);

private:
    void doLayout(Rect frame) override;
};

std::unique_ptr<TextField> textField(std::string defaultText = {});

} // namespace xui

#endif // AETHER_VIEW_H
