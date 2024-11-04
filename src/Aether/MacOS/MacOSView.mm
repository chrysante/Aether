#define AETHER_VIEW_IMPL

#include "Aether/View.h"

#include <cwchar>
#include <functional>
#include <optional>
#include <span>

#import <Appkit/Appkit.h>
#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>

#include "Aether/MacOS/MacOSUtil.h"
#include "Aether/ViewUtil.h"

using namespace xui;
using detail::PrivateViewKey;

// MARK: - Events

static void* ViewHandleKey = &ViewHandleKey;

static View* getView(NSView* view) {
    return getAssocPointer<View*>(view, ViewHandleKey);
}

template <typename V>
static V* getView(NSView* view) {
    return dynamic_cast<V*>(getView(view));
}

MouseButton toMouseButton(NSUInteger buttonNumber) {
    assert(buttonNumber <= 2);
    using enum MouseButton;
    return std::array{ Left, Right, Other }[buttonNumber];
}

MomentumPhase fromNS(NSEventPhase phase) {
    switch (phase) {
    case NSEventPhaseNone:
        return MomentumPhase::None;
    case NSEventPhaseBegan:
        return MomentumPhase::Began;
    case NSEventPhaseStationary:
        return MomentumPhase::Stationary;
    case NSEventPhaseChanged:
        return MomentumPhase::Changed;
    case NSEventPhaseEnded:
        return MomentumPhase::Ended;
    case NSEventPhaseCancelled:
        return MomentumPhase::Cancelled;
    case NSEventPhaseMayBegin:
        return MomentumPhase::MayBegin;
    }
    return MomentumPhase::None;
}

namespace {

struct EventTranslator {
    View& view;
    NSEvent* __weak event;

    xui::Window* window() const { return nullptr; };

    Vec2<double> locationInWindow() const {
        return fromAppkitCoords(event.locationInWindow,
                                event.window.frame.size.height);
    };

    Vec2<double> delta() const { return { event.deltaX, event.deltaY }; };

    Vec2<double> scrollingDelta() const {
        return { event.scrollingDeltaX, event.scrollingDeltaY };
    };

    MouseButton mouseButton() const {
        return toMouseButton(event.buttonNumber);
    };

    MomentumPhase momentum() const { return fromNS(event.momentumPhase); };

    template <typename E>
    E make(auto... args) const {
        return E((this->*args)()...);
    }

    EventUnion to(EventType type) {
        using T = EventTranslator;
        switch (type) {
        case EventType::ScrollEvent:
            return make<ScrollEvent>(&T::window, &T::locationInWindow,
                                     &T::scrollingDelta, &T::momentum);
        case EventType::MouseDownEvent:
            return make<MouseDownEvent>(&T::mouseButton, &T::window,
                                        &T::locationInWindow);
        case EventType::MouseUpEvent:
            return make<MouseUpEvent>(&T::mouseButton, &T::window,
                                      &T::locationInWindow);
        case EventType::MouseMoveEvent:
            return make<MouseMoveEvent>(&T::window, &T::locationInWindow,
                                        &T::delta);
        case EventType::MouseDragEvent:
            return make<MouseDragEvent>(&T::mouseButton, &T::window,
                                        &T::locationInWindow, &T::delta);
        case EventType::MouseEnterEvent:
            return make<MouseEnterEvent>(&T::window, &T::locationInWindow);
        case EventType::MouseExitEvent:
            return make<MouseExitEvent>(&T::window, &T::locationInWindow);
        default:
            assert(false);
        }
    }
};

} // namespace

struct View::EventImpl {
    static auto* getHandler(View& view, EventType type) {
        auto itr = view._eventHandlers.find(type);
        return itr != view._eventHandlers.end() ? &itr->second : nullptr;
    }
    static bool handleEvent(EventType type, View& view,
                            NSEvent __weak* nativeEvent) {
        auto event = EventTranslator{ view, nativeEvent }.to(type);
        if (event.visit([&](auto& event) { return view.onEvent(event); })) {
            return true;
        }
        auto* handler = getHandler(view, type);
        return handler && (*handler)(event);
    }
    static bool ignoreMouseEvents(View const& view) {
        return view._ignoreMouseEvents;
    }
};

static NSTrackingArea* makeTrackingArea(auto* __unsafe_unretained Self) {
    NSUInteger options = 0;
    if (test(Self.kind & MouseTrackingKind::Transition)) {
        options |= NSTrackingMouseEnteredAndExited;
    }
    if (test(Self.kind & MouseTrackingKind::Movement)) {
        options |= NSTrackingMouseMoved;
    }
    switch (Self.activity) {
    case MouseTrackingActivity::ActiveWindow:
        options |= NSTrackingActiveInKeyWindow;
        break;
    case MouseTrackingActivity::ActiveApp:
        options |= NSTrackingActiveInActiveApp;
        break;
    case MouseTrackingActivity::Always:
        options |= NSTrackingActiveAlways;
        break;
    }
    return [[NSTrackingArea alloc] initWithRect:Self.bounds
                                        options:options
                                          owner:Self
                                       userInfo:nil];
}

static void updateTrackingAreaFrame(auto* __unsafe_unretained Self) {
    if (!Self.hasTrackingArea) {
        return;
    }
    if (NSEqualRects(Self.bounds, Self.trackingArea.rect)) {
        return;
    }
    [Self updateTrackingArea:Self.kind activity:Self.activity];
}

static MouseTrackingActivity max(MouseTrackingActivity a,
                                 MouseTrackingActivity b) {
    return (MouseTrackingActivity)std::max((int)a, (int)b);
}

static void updateTrackingArea(auto* __unsafe_unretained Self,
                               MouseTrackingKind kind,
                               MouseTrackingActivity activity) {
    Self.kind |= kind;
    Self.activity = ::max(Self.activity, activity);
    [Self removeTrackingArea:Self.trackingArea];
    Self.trackingArea = [Self makeTrackingArea];
    [Self addTrackingArea:Self.trackingArea];
}

static void setHasTrackingArea(auto* __unsafe_unretained Self, bool value) {
    if (value) {
        if (!Self.trackingArea) {
            Self.trackingArea = [Self makeTrackingArea];
        }
    }
    else {
        [Self removeTrackingArea:Self.trackingArea];
        Self.trackingArea = nil;
    }
}

#define EVENT_TYPE_IMPL(Name, AppkitName)                                      \
    -(void)AppkitName: (NSEvent*)event {                                       \
        if (!View::EventImpl::handleEvent(EventType::Name, *getView(self),     \
                                          event))                              \
            [super AppkitName:event];                                          \
    }

/// This macro allows us to subclass any NSView and add custom event handling to
/// it. This way we don't have to add an additional subview to each view that we
/// want to handle events on
#define EVENT_VIEW_SUBCLASS(Name, Super)                                       \
    @interface Name: Super                                                     \
    @property MouseTrackingKind kind;                                          \
    @property MouseTrackingActivity activity;                                  \
    @property NSTrackingArea* trackingArea;                                    \
    @end                                                                       \
    @implementation Name                                                       \
    EVENT_TYPE_IMPL(ScrollEvent, scrollWheel)                                  \
    EVENT_TYPE_IMPL(MouseDownEvent, mouseDown)                                 \
    EVENT_TYPE_IMPL(MouseDownEvent, rightMouseDown)                            \
    EVENT_TYPE_IMPL(MouseDownEvent, otherMouseDown)                            \
    EVENT_TYPE_IMPL(MouseUpEvent, mouseUp)                                     \
    EVENT_TYPE_IMPL(MouseUpEvent, rightMouseUp)                                \
    EVENT_TYPE_IMPL(MouseUpEvent, otherMouseUp)                                \
    EVENT_TYPE_IMPL(MouseMoveEvent, mouseMoved)                                \
    EVENT_TYPE_IMPL(MouseDragEvent, mouseDragged)                              \
    EVENT_TYPE_IMPL(MouseDragEvent, rightMouseDragged)                         \
    EVENT_TYPE_IMPL(MouseDragEvent, otherMouseDragged)                         \
    EVENT_TYPE_IMPL(MouseEnterEvent, mouseEntered)                             \
    EVENT_TYPE_IMPL(MouseExitEvent, mouseExited)                               \
    -(void)setFrame: (NSRect)frame {                                           \
        [super setFrame:frame];                                                \
        ::updateTrackingAreaFrame(self);                                       \
    }                                                                          \
    -(NSTrackingArea*)makeTrackingArea {                                       \
        return ::makeTrackingArea(self);                                       \
    }                                                                          \
    -(void)updateTrackingArea:                                                 \
        (MouseTrackingKind)kind activity: (MouseTrackingActivity)activity {    \
        ::updateTrackingArea(self, kind, activity);                            \
    }                                                                          \
    -(BOOL)hasTrackingArea {                                                   \
        return !!self.trackingArea;                                            \
    }                                                                          \
    -(void)setHasTrackingArea: (BOOL)value {                                   \
        ::setHasTrackingArea(self, value);                                     \
    }                                                                          \
    -(BOOL)acceptsFirstMouse: (NSEvent*)event {                                \
        return YES;                                                            \
    }                                                                          \
    -(nullable NSView*)hitTest: (NSPoint)point {                               \
        if (View::EventImpl::ignoreMouseEvents(*getView(self))) return nil;    \
        return [super hitTest:point];                                          \
    }                                                                          \
    @end

// MARK: - Default view

EVENT_VIEW_SUBCLASS(EventView, NSView)

struct View::CustomImpl {
    static void draw(View& view, Rect frame) { view.draw(frame); }
    static bool clipsToBounds(View const& view) { return view.clipsToBounds(); }
};

@interface AetherDefaultView: EventView
@end
@implementation AetherDefaultView

- (void)viewWillMoveToWindow:(NSWindow*)window {
    if (self.wantsLayer) {
        self.layer.contentsScale = window.backingScaleFactor;
    }
}

- (void)layout {
    [super layout];
    View::CustomImpl::draw(*getView(self), {});
}

- (BOOL)clipsToBounds {
    return View::CustomImpl::clipsToBounds(*getView(self));
}
@end

void* detail::defaultNativeConstructor(ViewOptions const&) {
    AetherDefaultView* native = [[AetherDefaultView alloc] init];
    return retain(native);
}

// MARK: - View

View::~View() { release(nativeHandle()); }

xui::Point View::origin() const {
    NSView* view = transfer(nativeHandle());
    auto pos =
        fromAppkitCoords(view.frame.origin, view.superview.frame.size.height);
    pos.y -= view.frame.size.height;
    return pos;
}

xui::Size View::size() const {
    NSView* view = transfer(nativeHandle());
    return fromNSSize(view.frame.size);
}

void View::setNativeHandle(void* handle) {
    assert(!_nativeHandle && "Handle is already set");
    _nativeHandle = handle;
    if (handle) {
        setAssocPointer(transfer(handle), ViewHandleKey, this);
    }
}

void View::setSubviews(std::vector<std::unique_ptr<View>> views) {
    removeAllSubviews();
    NSView* native = transfer(nativeHandle());
    setSubviewsWeak(PrivateViewKey, std::move(views));
    for (auto* child: subviews()) {
        [native addSubview:transfer(child->nativeHandle())];
    }
}

void View::removeAllSubviews() {
    NSView* native = transfer(nativeHandle());
    NSArray* nativeSubviews = [native subviews];
    for (NSView* subview in nativeSubviews) {
        [subview removeFromSuperview];
    }
    _subviews.clear();
}

void View::orderFront() {
    NSView* __unsafe_unretained native = transfer(nativeHandle());
    NSView* __unsafe_unretained superview = native.superview;
    [superview
        sortSubviewsUsingFunction:[](__kindof NSView* __unsafe_unretained a,
                                     __kindof NSView* __unsafe_unretained b,
                                     void* nativeHandle) {
        NSView* __unsafe_unretained native = transfer(nativeHandle);
        if (a == native) {
            return NSOrderedDescending;
        }
        if (b == native) {
            return NSOrderedAscending;
        }
        return NSOrderedSame;
    }
                          context:nativeHandle()];
}

template <EventType ID, size_t Index = 0, auto... DerivedIDs>
struct DerivedEventTypeListImpl:
    std::conditional_t<csp::impl::IDIsConcrete<(EventType)Index> &&
                           csp::impl::ctIsaImpl(ID, (EventType)Index),
                       DerivedEventTypeListImpl<ID, Index + 1, DerivedIDs...,
                                                (EventType)Index>,
                       DerivedEventTypeListImpl<ID, Index + 1, DerivedIDs...>> {
};

template <EventType ID, auto... DerivedIDs>
struct DerivedEventTypeListImpl<ID, csp::impl::IDTraits<EventType>::count,
                                DerivedIDs...> {
    static constexpr size_t Count = sizeof...(DerivedIDs);
    static constexpr std::array<EventType, Count> value = { DerivedIDs... };
};

template <EventType ID>
static constexpr auto DerivedEventTypeList =
    DerivedEventTypeListImpl<ID>::value;

void View::installEventHandler(EventType type,
                               std::function<bool(EventUnion const&)> handler) {
    [&]<size_t... I>(std::index_sequence<I...>) {
        auto makeImpl = []<size_t J>() {
            return [](View& view,
                      std::function<bool(EventUnion const&)> const& handler) {
                auto list = DerivedEventTypeList<(EventType)J>;
                for (EventType ID: list) {
                    view._eventHandlers.insert_or_assign(ID, handler);
                }
            };
        };
        return std::array { +makeImpl.template operator()<I>()... };
    }(std::make_index_sequence<csp::impl::IDTraits<EventType>::count>{})[(
        size_t)type](*this, handler);
}

static SEL const UpdateTrackingAreaSelector =
    NSSelectorFromString(@"updateTrackingArea:activity:");

void View::trackMouseMovement(MouseTrackingKind kind,
                              MouseTrackingActivity activity) {
    NSView* native = transfer(nativeHandle());
    assert([native respondsToSelector:UpdateTrackingAreaSelector]);
    NSMethodSignature* signature =
        [native methodSignatureForSelector:UpdateTrackingAreaSelector];
    NSInvocation* invocation =
        [NSInvocation invocationWithMethodSignature:signature];
    [invocation setTarget:native];
    [invocation setSelector:UpdateTrackingAreaSelector];
    // Index 0 is self, index 1 is _cmd
    [invocation setArgument:&kind atIndex:2];
    [invocation setArgument:&activity atIndex:3];
    [invocation invoke];
}

bool View::setFrame(Rect frame) {
    NSView* view = transfer(nativeHandle());
    NSRect newFrame = toAppkitCoords(frame, view.superview.frame.size.height);
    if (!NSEqualRects(view.frame, newFrame)) {
        view.frame = newFrame;
        return true;
    }
    return false;
}

// MARK: - AggregateView

View* View::addSubview(std::unique_ptr<View> view) {
    NSView* native = transfer(nativeHandle());
    if (NSView* nativeChild = transfer(view->nativeHandle())) {
        [native addSubview:nativeChild];
    }
    view->_parent = this;
    _subviews.push_back(std::move(view));
    return _subviews.back().get();
}

// MARK: - StackView

StackView::StackView(Axis axis, std::vector<std::unique_ptr<View>> children):
    View({ .layoutModeX = LayoutMode::Flex,
           .layoutModeY = LayoutMode::Flex,
           .nativeConstructor = nativeConstructor }),
    axis(axis) {
    setSubviews(std::move(children));
}

void* StackView::nativeConstructor(ViewOptions const&) {
    NSView* view = [[EventView alloc] init];
    return retain(view);
}

// MARK: - ScrollView

EVENT_VIEW_SUBCLASS(EventScrollView, NSScrollView)

ScrollView::ScrollView(Axis axis, std::vector<std::unique_ptr<View>> children):
    View({ .layoutModeX = LayoutMode::Flex,
           .layoutModeY = LayoutMode::Flex,
           .nativeConstructor = std::bind_front(nativeConstructor, axis) }),
    axis(axis) {
    NSScrollView* native = transfer(nativeHandle());
    setSubviewsWeak(PrivateViewKey, std::move(children));
    for (auto* subview: subviews()) {
        [native.documentView addSubview:transfer(subview->nativeHandle())];
    }
}

void* ScrollView::nativeConstructor(Axis axis, ViewOptions const&) {
    NSView* content = [[NSView alloc] init];
    NSScrollView* scrollView = [[EventScrollView alloc] init];
    [scrollView setDocumentView:content];
    scrollView.hasHorizontalScroller = axis == Axis::X;
    scrollView.hasVerticalScroller = axis == Axis::Y;
    return retain(scrollView);
}

void ScrollView::setDocumentSize(Size size) {
    NSScrollView* view = transfer(nativeHandle());
    view.documentView.frame = { {}, toNSSize(size) };
}

void xui::applyModifier(NoBackgroundT, ScrollView& view) {
    NSScrollView* native = transfer(view.nativeHandle());
    native.drawsBackground = NO;
}

// MARK: - SplitView

EVENT_VIEW_SUBCLASS(EventSplitView, NSSplitView)

@interface AetherSplitView: EventSplitView
@property NSColor* divColor;
@property std::optional<double> divThickness;
@property SplitView* This;
@end

struct SplitView::Impl {
    static void didResizeSubviews(SplitView& view) { view.didResizeSubviews(); }
    static double constrainSplitPosition(SplitView const& view,
                                         double proposedPosition,
                                         size_t dividerIndex) {
        return view.constrainSplitPosition(proposedPosition, dividerIndex);
    }
};

@implementation AetherSplitView
- (NSColor*)dividerColor {
    if (self.divColor) {
        return self.divColor;
    }
    return [super dividerColor];
}
- (double)dividerThickness {
    if (self.divThickness) {
        return *self.divThickness;
    }
    return [super dividerThickness];
}
@end

@interface SplitViewDelegate: NSObject <NSSplitViewDelegate>
@end

@implementation SplitViewDelegate
- (void)splitViewDidResizeSubviews:(NSNotification*)notification {
    auto* This = ((AetherSplitView*)notification.object).This;
    SplitView::Impl::didResizeSubviews(*This);
}
- (void)splitView:(NSSplitView*)splitView
    resizeSubviewsWithOldSize:(NSSize)oldSize {
}
- (BOOL)splitView:(NSSplitView*)splitView canCollapseSubview:(NSView*)subview {
    auto* child = getView(subview);
    auto value = child->getAttribute<ViewAttributeKey::SplitViewCollapsable>();
    return value.value_or(false);
}
- (CGFloat)splitView:(NSSplitView*)splitView
    constrainSplitPosition:(CGFloat)proposedPosition
               ofSubviewAt:(NSInteger)dividerIndex {
    auto* This = ((AetherSplitView*)splitView).This;
    return SplitView::Impl::constrainSplitPosition(*This, proposedPosition,
                                                   dividerIndex);
}
@end

static SplitViewDelegate* const gSplitViewDelegate =
    [[SplitViewDelegate alloc] init];

SplitView::SplitView(Axis axis, std::vector<std::unique_ptr<View>> children):
    View({ .layoutModeX = LayoutMode::Flex,
           .layoutModeY = LayoutMode::Flex,
           .nativeConstructor =
               std::bind_front(nativeConstructor, this, axis) }),
    axis(axis) {
    setSubviews(std::move(children));
    setSplitterStyle(_splitterStyle);
}

void* SplitView::nativeConstructor(SplitView* This, Axis axis,
                                   ViewOptions const&) {
    AetherSplitView* view = [[AetherSplitView alloc] init];
    view.This = This;
    view.vertical = axis == Axis::X;
    [view setDelegate:gSplitViewDelegate];
    return retain(view);
}

static NSSplitViewDividerStyle toNS(SplitterStyle style) {
    using enum SplitterStyle;
    switch (style) {
    case Thin:
        return NSSplitViewDividerStyleThin;
    case Thick:
        return NSSplitViewDividerStyleThick;
    case Pane:
        return NSSplitViewDividerStylePaneSplitter;
    }
}

void SplitView::setSplitterStyle(SplitterStyle style) {
    _splitterStyle = style;
    NSSplitView* view = transfer(nativeHandle());
    view.dividerStyle = toNS(style);
}

void SplitView::setSplitterColor(std::optional<Color> color) {
    _splitterColor = color;
    AetherSplitView* view = transfer(nativeHandle());
    view.divColor = toNSColor(color);
}

void SplitView::setSplitterThickness(std::optional<double> thickness) {
    _splitterThickness = thickness;
    AetherSplitView* view = transfer(nativeHandle());
    view.divThickness = thickness;
}

double SplitView::sizeWithoutDividers() const {
    NSSplitView* view = transfer(nativeHandle());
    double totalSize = fromNSSize(view.frame.size)[axis];
    return totalSize - view.dividerThickness * (numSubviews() - 1);
}

bool SplitView::isChildCollapsed(size_t childIndex) const {
    NSSplitView* view = (NSSplitView*)transfer(nativeHandle());
    NSView* subview = transfer(subviewAt(childIndex)->nativeHandle());
    return [view isSubviewCollapsed:subview];
}

static void handleSplitViewResize(SplitViewResizeStrategy strat, double fracSum,
                                  std::span<double> fractions) {
    using enum SplitViewResizeStrategy;
    auto cutImpl = [&](double& frac) {
        double diff = 1 - fracSum;
        double newFrac = frac + diff;
        frac = std::max(0.0, newFrac);
    };
    switch (strat) {
    case Proportional:
        for (auto& frac: fractions) {
            frac /= fracSum;
        }
        break;
    case CutMin:
        cutImpl(fractions.front());
        break;
    case CutMax:
        cutImpl(fractions.back());
        break;
    case None:
        break;
    }
}

void SplitView::didResizeSubviews() {
    AetherSplitView* view = (AetherSplitView*)transfer(nativeHandle());
    if (childFractions.empty()) {
        return;
    }
    auto frame = fromAppkitCoords(view.frame, view.superview.frame.size.height);
    double totalSize = sizeWithoutDividers();
    double fracSum = 0;
    for (size_t i = 0; i < numSubviews(); ++i) {
        auto* child = subviewAt(i);
        double size = child->size()[axis];
        double frac = size / totalSize;
        childFractions[i] = frac;
        if (!isChildCollapsed(i)) {
            fracSum += frac;
        }
    }
    if (fracSum != 1.0) {
        handleSplitViewResize(resizeStrategy(), fracSum, childFractions);
    }
    layout(frame);
}

double SplitView::constrainSplitPosition(double proposedPosition,
                                         size_t index) const {
    auto* left = subviewAt(index);
    auto* right = subviewAt(index + 1);
    // Must transform the position again because of the flipped coordinate
    // system
    Point leftPosition = left->origin();
    leftPosition.y += left->size().height();
    leftPosition.y = size().height() - leftPosition.y;
    double currentPosition = leftPosition[axis] + left->size()[axis];
    if (left->size()[axis] <= left->minSize()[axis] &&
        right->size()[axis] <= right->minSize()[axis])
    {
        return currentPosition;
    }
    double offset = proposedPosition - currentPosition;
    double leftNewSize = left->size()[axis] + offset;
    if (left->minSize()[axis] > leftNewSize) {
        return leftPosition[axis] + left->minSize()[axis];
    }
    double rightNewSize = right->size()[axis] - offset;
    if (right->minSize()[axis] > rightNewSize) {
        return currentPosition + right->size()[axis] - right->minSize()[axis];
    }
    return proposedPosition;
}

void SplitView::doLayout(Rect frame) {
    NSSplitView* view = transfer(nativeHandle());
    double dividerThickness = view.dividerThickness;
    if (setFrame(frame) && !childFractions.empty()) {
        return;
    }
    double totalSize = sizeWithoutDividers();
    if (childFractions.empty()) {
        double frac = 1.0 / numSubviews();
        childFractions.resize(numSubviews(), frac);
    }
    double offset = 0;
    for (size_t i = 0; i < numSubviews(); ++i) {
        if (isChildCollapsed(i)) {
            offset += dividerThickness;
            continue;
        }
        auto* child = subviewAt(i);
        double frac = childFractions[i];
        assert(frac >= 0.0);
        double childSize = totalSize * frac;
        Rect childFrame = { Point(axis, offset), frame.size() };
        childFrame.size()[axis] = childSize;
        if (axis == Axis::Y) {
            // We need to do this coordinate transform dance because here AppKit
            // uses top-left -> bottom-right coordinates, unlike everywhere else
            childFrame.origin().y =
                frame.height() - childFrame.height() - childFrame.origin().y;
        }
        child->layout(childFrame);
        offset += childSize + dividerThickness;
    }
}

// MARK: - TabView

static NSTabPosition toNS(TabPosition pos) {
    using enum TabPosition;
    switch (pos) {
    case None:
        return NSTabPositionNone;
    case Top:
        return NSTabPositionTop;
    case Left:
        return NSTabPositionLeft;
    case Bottom:
        return NSTabPositionBottom;
    case Right:
        return NSTabPositionRight;
    }
}

static NSTabViewBorderType toNS(TabViewBorder border) {
    using enum TabViewBorder;
    switch (border) {
    case None:
        return NSTabViewBorderTypeNone;
    case Line:
        return NSTabViewBorderTypeLine;
    case Bezel:
        return NSTabViewBorderTypeBezel;
    }
}

EVENT_VIEW_SUBCLASS(EventTabView, NSTabView)

TabView::TabView(std::vector<TabViewElement> elems):
    View({ .layoutModeX = LayoutMode::Flex,
           .layoutModeY = LayoutMode::Flex,
           .nativeConstructor = nativeConstructor }),
    elements(std::move(elems)) {
    NSTabView* native = transfer(nativeHandle());
    native.tabPosition = toNS(_tabPosition);
    native.tabViewBorderType = toNS(_border);
    for (auto& [title, child]: elements) {
        child->_parent = this;
        NSTabViewItem* item = [[NSTabViewItem alloc] init];
        item.view = transfer(child->nativeHandle());
        item.label = toNSString(title);
        [native addTabViewItem:item];
    }
}

void* TabView::nativeConstructor(ViewOptions const&) {
    NSTabView* view = [[EventTabView alloc] init];
    return retain(view);
}

void TabView::setTabPosition(TabPosition position) {
    _tabPosition = position;
    NSTabView* view = transfer(nativeHandle());
    view.tabPosition = toNS(_tabPosition);
}

void TabView::setBorder(TabViewBorder border) {
    _border = border;
    NSTabView* view = transfer(nativeHandle());
    view.tabViewBorderType = toNS(_border);
}

void TabView::doLayout(Rect frame) {
    setFrame(frame);
    if (elements.empty()) {
        return;
    }
    NSTabView* native = transfer(nativeHandle());
    NSView* selected = native.selectedTabViewItem.view;
    auto childFrame = fromAppkitCoords(selected.frame, frame.height());
    for (auto& [title, child]: elements) {
        child->layout(childFrame);
    }
}

// MARK: - Button

@interface NSButton (BlockAction)
- (void)setActionBlock:(void (^)(void))actionBlock;
@end
static void const* NSButtonBlockKey = &NSButtonBlockKey;
@implementation NSButton (BlockAction)
- (void)setActionBlock:(void (^)(void))actionBlock {
    objc_setAssociatedObject(self, NSButtonBlockKey, actionBlock,
                             OBJC_ASSOCIATION_COPY_NONATOMIC);
    [self setTarget:self];
    [self setAction:@selector(executeActionBlock)];
}
- (void)executeActionBlock {
    void (^actionBlock)(void) =
        objc_getAssociatedObject(self, NSButtonBlockKey);
    if (actionBlock) {
        actionBlock();
    }
}
@end

[[maybe_unused]]
static xui::Size computeSize(NSString* text) {
    NSFont* font = [NSFont systemFontOfSize:[NSFont systemFontSize]];
    NSDictionary* attributes = @{ NSFontAttributeName : font };
    NSSize textSize = [text sizeWithAttributes:attributes];
    return { textSize.width, textSize.height };
}

static NSButtonType toNS(ButtonType type) {
    using enum ButtonType;
    switch (type) {
    case Default:
        return NSButtonTypeMomentaryLight;
    case Toggle:
        return NSButtonTypePushOnPushOff;
    case Switch:
        return NSButtonTypeSwitch;
    case Radio:
        return NSButtonTypeRadio;
    }
}

static NSBezelStyle toNS(BezelStyle style) {
    using enum BezelStyle;
    switch (style) {
    case Push:
        return NSBezelStylePush;
    case PushFlexHeight:
        return NSBezelStyleFlexiblePush;
    case Circular:
        return NSBezelStyleCircular;
    case Help:
        return NSBezelStyleHelpButton;
    case SmallSquare:
        return NSBezelStyleSmallSquare;
    case Toolbar:
        return NSBezelStyleToolbar;
    case Badge:
        return NSBezelStyleBadge;
    }
}

static std::string_view trimToFirstCharacter(std::string_view s) {
    if (s.empty()) {
        return s;
    }
    size_t charLength = std::mbrlen(s.data(), s.size(), nullptr);
    return { s.data(), charLength };
}

NSString* makeButtonTitle(std::string_view title, ButtonType type,
                          BezelStyle style) {
    using enum BezelStyle;
    if (type == ButtonType::Default || type == ButtonType::Toggle) {
        switch (style) {
        case Circular:
            title = trimToFirstCharacter(title);
            break;
        case Help:
            title = {};
            break;
        default:
            break;
        }
    }
    return toNSString(title);
}

EVENT_VIEW_SUBCLASS(EventButtonView, NSButton)

static void* buttonCtor(ViewOptions const&) {
    NSButton* button = [[EventButtonView alloc] init];
    return retain(button);
}

ButtonView::ButtonView(std::string label, std::function<void()> action,
                       ButtonType type):
    View({ .minSize = { 80, 34 }, .nativeConstructor = buttonCtor }),
    _type(type),
    _label(std::move(label)),
    _action(std::move(action)) {
    NSButton* button = transfer(nativeHandle());
    button.buttonType = toNS(type);
    setBezelStyle(_bezelStyle);
    [button setActionBlock:^{
        if (_action) {
            _action();
        }
    }];
    setPreferredSize(fromNSSize(button.intrinsicContentSize));
}

void ButtonView::setBezelStyle(BezelStyle style) {
    _bezelStyle = style;
    NSButton* button = transfer(nativeHandle());
    button.bezelStyle = toNS(_bezelStyle);
    setLabel(std::move(_label));
}

void ButtonView::setLabel(std::string label) {
    _label = std::move(label);
    NSButton* button = transfer(nativeHandle());
    [button setTitle:makeButtonTitle(_label, buttonType(), bezelStyle())];
}

// MARK: - Switch

EVENT_VIEW_SUBCLASS(EventSwitchView, NSSwitch)

static void* switchCtor(ViewOptions const&) {
    NSSwitch* view = [[EventSwitchView alloc] init];
    return retain(view);
}

SwitchView::SwitchView(): View({ .nativeConstructor = switchCtor }) {
    NSView* view = transfer(nativeHandle());
    setMinSize(fromNSSize(view.intrinsicContentSize));
}

// MARK: - TextField

EVENT_VIEW_SUBCLASS(EventTextField, NSTextField)

static void* textFieldCtor(ViewOptions const&) {
    NSTextField* view = [[EventTextField alloc] init];
    return retain(view);
}

TextFieldView::TextFieldView(std::string defaultText):
    View({ .minSize = { 80, 32 },
           .layoutModeX = LayoutMode::Flex,
           .layoutModeY = LayoutMode::Static,
           .nativeConstructor = textFieldCtor }) {
    NSTextField* view = transfer(nativeHandle());
    view.stringValue = toNSString(defaultText);
    view.bezelStyle = NSTextFieldRoundedBezel;
    setAttribute<ViewAttributeKey::PaddingX>(6);
    setAttribute<ViewAttributeKey::PaddingY>(6);
}

void TextFieldView::setText(std::string /* text */) {
    NSTextField* field = transfer(nativeHandle());
    assert(field && false);
}

std::string TextFieldView::getText() const {
    NSTextField* field = transfer(nativeHandle());
    return toStdString([field stringValue]);
}

// MARK: - LabelView

static void* labelCtor(NSString* text, ViewOptions const&) {
    NSTextField* view = [EventTextField labelWithString:text];
    return retain(view);
}

LabelView::LabelView(std::string text):
    View({ .minSize = { 80, 22 },
           .layoutModeX = LayoutMode::Flex,
           .layoutModeY = LayoutMode::Static,
           .nativeConstructor =
               std::bind_front(labelCtor, toNSString(text)) }) {}

void LabelView::setText(std::string text) {
    NSTextField* field = transfer(nativeHandle());
    field.stringValue = toNSString(text);
}

// MARK: - ProgressIndicatorView

EVENT_VIEW_SUBCLASS(EventProgressIndicator, NSProgressIndicator)

static NSProgressIndicatorStyle toNS(ProgressIndicatorView::Style style) {
    using enum ProgressIndicatorView::Style;
    switch (style) {
    case Bar:
        return NSProgressIndicatorStyleBar;
    case Spinner:
        return NSProgressIndicatorStyleSpinning;
    }
}

static void* progressCtor(ProgressIndicatorView::Style style,
                          ViewOptions const&) {
    NSProgressIndicator* view = [[EventProgressIndicator alloc] init];
    view.style = toNS(style);
    [view startAnimation:nil];
    return retain(view);
}

static xui::Size progressMinSize(ProgressIndicatorView::Style style) {
    using enum ProgressIndicatorView::Style;
    switch (style) {
    case Bar:
        return { 0, 10 };
    case Spinner:
        return { 20, 20 };
    }
}

ProgressIndicatorView::ProgressIndicatorView(Style style):
    View({ .minSize = progressMinSize(style),
           .layoutModeX = style == Bar ? LayoutMode::Flex : LayoutMode::Static,
           .layoutModeY = LayoutMode::Static,
           .nativeConstructor = std::bind_front(progressCtor, style) }) {}

// MARK: - ColorView

@interface FlatColorView: EventView
@property(nonatomic, strong) NSColor* color;
@end
@implementation FlatColorView
- (instancetype)initWithColor:(NSColor*)color {
    self = [super init];
    if (!self) {
        return nil;
    }
    self.color = color;
    return self;
}
- (void)drawRect:(NSRect)dirtyRect {
    NSRect drawRect = NSIntersectionRect(dirtyRect, { {}, self.frame.size });
    [[NSColor blackColor] setFill];
    NSRectFill(drawRect);
    [self.color setFill];
    NSRectFill(NSInsetRect(drawRect, 2, 2));
    NSString* text = [NSString stringWithFormat:@"Width: %f\nHeight: %f",
                                                self.frame.size.width,
                                                self.frame.size.height];
    NSDictionary* attributes = @{
        NSFontAttributeName : [NSFont systemFontOfSize:12],
        NSForegroundColorAttributeName : [NSColor blackColor]
    };
    NSRect textRect = NSInsetRect(drawRect, 15, 15);
    [text drawInRect:textRect withAttributes:attributes];
}
@end

static void* colorViewCtor(NSColor* color, ViewOptions const&) {
    FlatColorView* view = [[FlatColorView alloc] initWithColor:color];
    return retain(view);
}

ColorView::ColorView(Color const& color):
    View({ .layoutModeX = LayoutMode::Flex,
           .layoutModeY = LayoutMode::Flex,
           .nativeConstructor =
               std::bind_front(colorViewCtor, toNSColor(color)) }) {}

void ColorView::doLayout(Rect frame) { setFrame(frame); }

EVENT_VIEW_SUBCLASS(AetherVisualEffectView, NSVisualEffectView)

VisualEffectView::VisualEffectView(VisualEffectBlendMode blendMode,
                                   std::unique_ptr<View> subview):
    View({ .layoutModeX = LayoutMode::Flex,
           .layoutModeY = LayoutMode::Flex,
           .nativeConstructor =
               std::bind_front(nativeConstructor, blendMode) }) {
    addSubview(std::move(subview));
}

static NSVisualEffectBlendingMode toNS(VisualEffectBlendMode mode) {
    using enum VisualEffectBlendMode;
    switch (mode) {
    case BehindWindow:
        return NSVisualEffectBlendingModeBehindWindow;
    case WithinWindow:
        return NSVisualEffectBlendingModeWithinWindow;
    }
}

void* VisualEffectView::nativeConstructor(VisualEffectBlendMode blendMode,
                                          ViewOptions const&) {
    AetherVisualEffectView* native = [[AetherVisualEffectView alloc] init];
    native.blendingMode = toNS(blendMode);
    native.material = NSVisualEffectMaterialSidebar; // For now
    return retain(native);
}

void VisualEffectView::doLayout(Rect frame) {
    setFrame(frame);
    for (auto* view: subviews()) {
        view->layout({ {}, frame.size() });
    }
}
