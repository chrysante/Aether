#include "Aether/View.h"

#include <cassert>

#include <range/v3/view.hpp>

#include "Aether/ViewUtil.h"

using namespace xui;

void View::layout(Rect frame) { doLayout(frame); }

void View::setAttribute(detail::ViewAttributeKey key, std::any value) {
    _attribMap.insert_or_assign(key, std::move(value));
}

std::any View::getAttribute(detail::ViewAttributeKey key) const {
    auto itr = _attribMap.find(key);
    return itr != _attribMap.end() ? itr->second : std::any();
}

AggregateView::AggregateView(Axis axis,
                             std::vector<std::unique_ptr<View>> children):
    _axis(axis), _children(std::move(children)) {
    for (auto& child: _children) {
        child->_parent = this;
    }
}

static constexpr Size SpacerMinSize = { 5, 5 };

SpacerView::SpacerView() {
    _minSize = SpacerMinSize;
    _layoutMode = LayoutMode::Flex;
}

std::unique_ptr<SpacerView> xui::Spacer() {
    return std::make_unique<SpacerView>();
}

namespace {

struct StackLayoutConstraints {
    size_t numFlexChildren = 0;
    double totalMinSize = 0;
};

} // namespace

template <Axis A>
static StackLayoutConstraints gatherContraints(auto&& children) {
    StackLayoutConstraints result{};
    for (View const* child: children) {
        switch (get<(size_t)A>(child->layoutMode())) {
        case LayoutMode::Static:
            result.totalMinSize += get<(size_t)A>(child->minSize());
            break;
        case LayoutMode::Flex:
            ++result.numFlexChildren;
            break;
        }
    }
    return result;
}

namespace {

struct ChildrenLayoutOptions {
    Vec2<bool> fillAvailSpace;
};

} // namespace

template <Axis A>
static Rect layoutChildrenXY(auto&& children, Rect frame,
                             ChildrenLayoutOptions opt) {
    static_assert(A != Axis::Z);
    auto constraints = gatherContraints<A>(children);
    double flexSpace =
        std::max(0.0, get<(size_t)A>(frame.size()) - constraints.totalMinSize);
    double cursor = 0;
    Rect total{};
    for (View* child: children) {
        Size prefSize = child->preferredSize();
        auto layoutMode = child->layoutMode();
        for (auto [index, mode]: layoutMode | ranges::views::enumerate) {
            if (mode == LayoutMode::Static || !opt.fillAvailSpace[index]) {
                continue;
            }
            if ((Axis)index == A) {
                prefSize[index] = flexSpace / constraints.numFlexChildren;
            }
            else {
                prefSize[index] = get<1 - (size_t)A>(frame.size());
            }
        }
        Size childSize = clamp(prefSize, child->minSize(), child->maxSize());
        Rect childRect{ Position(A, cursor), childSize };
        child->layout(childRect);
        cursor += get<(size_t)A>(childSize);
        total = merge(total, childRect);
    }
    return total;
}

void StackView::doLayout(Rect frame) {
    setFrame(frame);
    auto childrenView =
        _children | ranges::views::transform([](auto& c) { return c.get(); });
    dispatchAxis(axis(), [&]<Axis A>(std::integral_constant<Axis, A>) {
        if constexpr (A == Axis::Z) {
            assert(false);
        }
        else {
            layoutChildrenXY<A>(childrenView, frame,
                                { .fillAvailSpace = true });
        }
    });
}

std::unique_ptr<StackView> xui::HStack(UniqueVector<View> children) {
    return std::make_unique<StackView>(Axis::X, std::move(children));
}

std::unique_ptr<StackView> xui::VStack(UniqueVector<View> children) {
    return std::make_unique<StackView>(Axis::Y, std::move(children));
}

void ScrollView::doLayout(Rect frame) {
    setFrame(frame);
    auto childrenView =
        _children | ranges::views::transform([](auto& c) { return c.get(); });
    dispatchAxis(axis(), [&]<Axis A>(std::integral_constant<Axis, A>) {
        if constexpr (A == Axis::Z) {
            assert(false);
        }
        else {
            Rect total =
                layoutChildrenXY<A>(childrenView, frame,
                                    { .fillAvailSpace{ flip(A), true } });
            setDocumentSize(max(total.size(), frame.size()));
        }
    });
}

std::unique_ptr<ScrollView> xui::VScrollView(UniqueVector<View> children) {
    return std::make_unique<ScrollView>(Axis::Y, std::move(children));
}

std::unique_ptr<ScrollView> xui::HScrollView(UniqueVector<View> children) {
    return std::make_unique<ScrollView>(Axis::X, std::move(children));
}

std::unique_ptr<SplitView> xui::HSplit(UniqueVector<View> children) {
    return std::make_unique<SplitView>(Axis::X, std::move(children));
}

std::unique_ptr<SplitView> xui::VSplit(UniqueVector<View> children) {
    return std::make_unique<SplitView>(Axis::Y, std::move(children));
}

std::unique_ptr<ButtonView> xui::Button(std::string label,
                                        std::function<void()> action) {
    return std::make_unique<ButtonView>(std::move(label), std::move(action));
}

std::unique_ptr<TextFieldView> xui::TextField(std::string defaultText) {
    return std::make_unique<TextFieldView>(std::move(defaultText));
}

std::unique_ptr<LabelView> xui::Label(std::string text) {
    return std::make_unique<LabelView>(text);
}
