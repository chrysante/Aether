#include "Aether/View.h"

#include <cassert>

using namespace xui;

static constexpr Size SpacerMinSize = { 5, 5 };

Spacer::Spacer() {
    _minSize = SpacerMinSize;
    _layoutMode = LayoutMode::Flex;
}

namespace {

struct StackLayoutConstraints {
    std::vector<View*> flexChildren;
    double totalMinSize = 0;
};

} // namespace

auto makeSizeAccessor(Axis axis) {
    switch (axis) {
    case Axis::X:
        return &Size::width;
    case Axis::Y:
        return &Size::height;
    default:
        assert(false);
    }
}

static StackLayoutConstraints gatherContraints(Axis axis, auto&& children) {
    StackLayoutConstraints result{};
    for (auto& child: children) {
        if (child->layoutMode() == LayoutMode::Flex) {
            result.flexChildren.push_back(child.get());
            continue;
        }
        result.totalMinSize += child->minSize().*makeSizeAccessor(axis);
    }
    return result;
}

void StackView::doLayout(Rect frame) {
    setFrame(frame);
    layoutChildren(frame);
}

void StackView::layoutChildren(Rect frame) {
    auto constraints = gatherContraints(axis(), _children);
    auto sizeAcc = makeSizeAccessor(axis());
    double flexSpace =
        std::max(0.0, frame.size.*sizeAcc - constraints.totalMinSize);
    double cursor = 0;
    for (size_t i = 0; i < _children.size(); ++i) {
        auto* child = _children[i].get();
        double prefSize = [&] {
            using enum LayoutMode;
            switch (child->layoutMode()) {
            case Static:
                return child->preferredSize().*sizeAcc;
            case Flex:
                return flexSpace / constraints.flexChildren.size();
            }
        }();
        double childSize = std::clamp(prefSize, child->minSize().*sizeAcc,
                                      child->maxSize().*sizeAcc);
        switch (axis()) {
        case Axis::X: { // HStack
            Rect childRect{ Position{ cursor, 0 },
                            Size{ childSize, frame.size.height } };
            child->layout(childRect);
            cursor += childSize;
            break;
        }
        case Axis::Y: { // VStack
            Rect childRect{ Position{ 0, cursor },
                            Size{ frame.size.width, childSize } };
            child->layout(childRect);
            cursor += childSize;
            break;
        }
        case Axis::Z:
            child->layout(frame);
            break;
        }
    }
}

std::unique_ptr<StackView> xui::hStack(UniqueVector<View> children) {
    return std::make_unique<StackView>(Axis::X, std::move(children));
}

std::unique_ptr<StackView> xui::vStack(UniqueVector<View> children) {
    return std::make_unique<StackView>(Axis::Y, std::move(children));
}

std::unique_ptr<ScrollView> xui::scrollView(UniqueVector<View> children) {
    return std::make_unique<ScrollView>(Axis::Y, std::move(children));
}

std::unique_ptr<Button> xui::button(std::string label,
                                    std::function<void()> action) {
    return std::make_unique<Button>(std::move(label), std::move(action));
}

std::unique_ptr<TextField> xui::textField(std::string defaultText) {
    return std::make_unique<TextField>(std::move(defaultText));
}
