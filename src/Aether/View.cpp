#include "Aether/View.h"

#include <cassert>

#include <range/v3/view.hpp>

#include "Aether/ViewUtil.h"

using namespace xui;

using namespace detail::viewProperties;

View::View(Vec2<LayoutMode> layoutMode, MinSize minSize, PrefSize prefSize,
           MaxSize maxSize):
    _layoutMode(layoutMode),
    _minSize(minSize.value),
    _maxSize(maxSize.value),
    _prefSize(prefSize.value) {}

void View::layout(Rect frame) {
    if (auto padding = getAttribute<ViewAttributeKey::PaddingX>()) {
        frame.pos().x += *padding;
        frame.width() -= 2 * *padding;
    }
    if (auto padding = getAttribute<ViewAttributeKey::PaddingY>()) {
        frame.pos().y += *padding;
        frame.height() -= 2 * *padding;
    }
    doLayout(frame);
}

void View::setAttributeImpl(ViewAttributeKey key, std::any value) {
    _attribMap.insert_or_assign(key, std::move(value));
}

void View::clearAttributeImpl(ViewAttributeKey key) { _attribMap.erase(key); }

std::any View::getAttributeImpl(ViewAttributeKey key) const {
    auto itr = _attribMap.find(key);
    return itr != _attribMap.end() ? itr->second : std::any();
}

AggregateView::AggregateView(Axis axis,
                             std::vector<std::unique_ptr<View>> children,
                             Vec2<LayoutMode> layoutMode,
                             detail::MinSize minSize, detail::PrefSize prefSize,
                             detail::MaxSize maxSize):
    View(layoutMode, minSize, prefSize, maxSize),
    _axis(axis),
    _children(std::move(children)) {
    for (auto& child: _children) {
        child->_parent = this;
    }
}

static constexpr Size SpacerMinSize = { 5, 5 };

SpacerView::SpacerView():
    View({ LayoutMode::Flex, LayoutMode::Flex }, MinSize(SpacerMinSize)) {}

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
        switch (child->layoutMode()[A]) {
        case LayoutMode::Static:
            result.totalMinSize += child->minSize()[A];
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

template <typename T>
static T orDefault(std::optional<T> const& o) {
    return o.value_or(T{});
}

static double computeAlignedPosition(double childSize, double parentSize,
                                     auto align) {
    double sizeDiff = parentSize - childSize;
    using enum AlignX;
    switch ((AlignX)align) {
    case Left:
        return 0.0;
    case Center:
        return sizeDiff / 2;
    case Right:
        return sizeDiff;
    }
}

template <Axis A>
static Position computeAlignedPosition(View const& child, Size childSize,
                                       Size parentSize, double cursor) {
    static_assert(A != Axis::Z);
    constexpr ViewAttributeKey Key = A == Axis::X ? ViewAttributeKey::AlignY :
                                                    ViewAttributeKey::AlignX;
    auto B = flip(A);
    Position pos{};
    pos[A] = cursor;
    pos[B] = computeAlignedPosition(childSize[B], parentSize[B],
                                    orDefault(child.getAttribute<Key>()));
    return pos;
}

static Position computeAlignedPositionZ(View const& child, Size childSize,
                                        Size parentSize) {
    return {
        computeAlignedPosition(
            childSize[0], parentSize[0],
            orDefault(child.getAttribute<ViewAttributeKey::AlignX>())),
        computeAlignedPosition(
            childSize[1], parentSize[1],
            orDefault(child.getAttribute<ViewAttributeKey::AlignY>())),
    };
}

template <Axis A>
static Rect layoutChildrenXY(auto&& children, Rect frame,
                             ChildrenLayoutOptions opt) {
    static_assert(A != Axis::Z);
    auto constraints = gatherContraints<A>(children);
    double flexSpace =
        std::max(0.0, frame.size()[A] - constraints.totalMinSize);
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
                prefSize[index] = frame.size()[flip(A)];
            }
        }
        Size childSize = clamp(prefSize, child->minSize(), child->maxSize());
        Position childPosition =
            computeAlignedPosition<A>(*child, childSize, frame.size(), cursor);
        Rect childRect{ childPosition, childSize };
        child->layout(childRect);
        cursor += childSize[A];
        total = merge(total, childRect);
    }
    return total;
}

static Rect layoutChildrenZ(auto&& children, Rect frame) {
    Rect total{};
    for (View* child: children) {
        Size prefSize = child->preferredSize();
        auto layoutMode = child->layoutMode();
        for (auto [index, mode]: layoutMode | ranges::views::enumerate) {
            if (mode == LayoutMode::Static) {
                continue;
            }
            prefSize[index] = frame.size()[index];
        }
        Size childSize = clamp(prefSize, child->minSize(), child->maxSize());
        Position childPosition =
            computeAlignedPositionZ(*child, childSize, frame.size());
        Rect childRect{ childPosition, childSize };
        child->layout(childRect);
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
            layoutChildrenZ(childrenView, frame);
        }
        else {
            layoutChildrenXY<A>(childrenView, frame,
                                { .fillAvailSpace = { true, true } });
        }
    });
}

std::unique_ptr<StackView> xui::HStack(UniqueVector<View> children) {
    return std::make_unique<StackView>(Axis::X, std::move(children));
}

std::unique_ptr<StackView> xui::VStack(UniqueVector<View> children) {
    return std::make_unique<StackView>(Axis::Y, std::move(children));
}

std::unique_ptr<StackView> xui::ZStack(UniqueVector<View> children) {
    return std::make_unique<StackView>(Axis::Z, std::move(children));
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

std::unique_ptr<TabView> xui::Tab(MoveOnlyVector<TabViewElement> elements) {
    return std::make_unique<TabView>(std::move(elements));
}

std::unique_ptr<ButtonView> xui::Button(std::string label,
                                        std::function<void()> action) {
    return std::make_unique<ButtonView>(std::move(label), std::move(action),
                                        ButtonType::Default);
}

std::unique_ptr<ButtonView> xui::ToggleButton(std::string label,
                                              std::function<void()> action) {
    return std::make_unique<ButtonView>(std::move(label), std::move(action),
                                        ButtonType::Toggle);
}

std::unique_ptr<ButtonView> xui::SwitchButton(std::string label,
                                              std::function<void()> action) {
    return std::make_unique<ButtonView>(std::move(label), std::move(action),
                                        ButtonType::Switch);
}

std::unique_ptr<ButtonView> xui::RadioButton(std::string label,
                                             std::function<void()> action) {
    return std::make_unique<ButtonView>(std::move(label), std::move(action),
                                        ButtonType::Radio);
}

std::unique_ptr<ProgressIndicatorView> xui::ProgressBar() {
    return std::make_unique<ProgressIndicatorView>(ProgressIndicatorView::Bar);
}

std::unique_ptr<ProgressIndicatorView> xui::ProgressSpinner() {
    return std::make_unique<ProgressIndicatorView>(
        ProgressIndicatorView::Spinner);
}

std::unique_ptr<TextFieldView> xui::TextField(std::string defaultText) {
    return std::make_unique<TextFieldView>(std::move(defaultText));
}

std::unique_ptr<LabelView> xui::Label(std::string text) {
    return std::make_unique<LabelView>(text);
}
