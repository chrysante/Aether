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
using namespace detail::viewProperties;
using detail::PrivateViewKey;

// MARK: - View

View::~View() { release(nativeHandle()); }

Position View::position() const {
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

static void* ViewHandleKey = &ViewHandleKey;

void View::setNativeHandle(void* handle) {
    _nativeHandle = handle;
    setAssocPointer(transfer(handle), ViewHandleKey, this);
}

static View* getView(NSView* view) {
    return getAssocPointer<View*>(view, ViewHandleKey);
}

template <typename V>
static V* getView(NSView* view) {
    return dynamic_cast<V*>(getView(view));
}

void View::requestLayout() {
    NSView* view = transfer(nativeHandle());
    [view setNeedsLayout:true];
}

void View::setSubviews(std::vector<std::unique_ptr<View>> views) {
    NSView* native = transfer(nativeHandle());
    NSArray* nativeSubviews = [native subviews];
    for (NSView* subview in nativeSubviews) {
        [subview removeFromSuperview];
    }
    setSubviewsWeak(PrivateViewKey, std::move(views));
    for (auto* child: subviews()) {
        [native addSubview:transfer(child->nativeHandle())];
    }
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

// MARK: - Events

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
    static bool handleEvent(EventType type, View& view, NSEvent __weak* event) {
        auto* handler = getHandler(view, type);
        if (!handler) return false;
        return (*handler)(EventTranslator{ view, event }.to(type));
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
    @end

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

EVENT_VIEW_SUBCLASS(EventView, NSView)

StackView::StackView(Axis axis, std::vector<std::unique_ptr<View>> children):
    View(PrivateViewKey, { LayoutMode::Flex, LayoutMode::Flex }), axis(axis) {
    NSView* view = [[EventView alloc] init];
    setNativeHandle(retain(view));
    setSubviews(std::move(children));
}

// MARK: - ScrollView

EVENT_VIEW_SUBCLASS(EventScrollView, NSScrollView)

ScrollView::ScrollView(Axis axis, std::vector<std::unique_ptr<View>> children):
    View(PrivateViewKey, { LayoutMode::Flex, LayoutMode::Flex }), axis(axis) {
    setSubviewsWeak(PrivateViewKey, std::move(children));
    NSView* content = [[NSView alloc] init];
    NSScrollView* scrollView = [[EventScrollView alloc] init];
    for (auto* child: subviews()) {
        NSView* childView = transfer(child->nativeHandle());
        [content addSubview:childView];
    }
    [scrollView setDocumentView:content];
    scrollView.hasHorizontalScroller = axis == Axis::X;
    scrollView.hasVerticalScroller = axis == Axis::Y;
    setNativeHandle(retain(scrollView));
}

void ScrollView::setDocumentSize(Size size) {
    NSScrollView* view = transfer(nativeHandle());
    view.documentView.frame = { {}, toNSSize(size) };
}

// MARK: - SplitView

EVENT_VIEW_SUBCLASS(EventSplitView, NSSplitView)

@interface AetherSplitView: EventSplitView
@property NSColor* divColor;
@property std::optional<double> divThickness;
@property SplitView* This;
@end

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
    This->didResizeSubviews();
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
    return This->constrainSplitPosition(proposedPosition, dividerIndex);
}
@end

static SplitViewDelegate* const gSplitViewDelegate =
    [[SplitViewDelegate alloc] init];

SplitView::SplitView(Axis axis, std::vector<std::unique_ptr<View>> children):
    View(PrivateViewKey, { LayoutMode::Flex, LayoutMode::Flex }), axis(axis) {
    setSubviewsWeak(PrivateViewKey, std::move(children));
    AetherSplitView* view = [[AetherSplitView alloc] init];
    view.arrangesAllSubviews = NO;
    view.This = this;
    view.vertical = axis == Axis::X;
    for (auto* child: subviews()) {
        [view addArrangedSubview:transfer(child->nativeHandle())];
    }
    setNativeHandle(retain(view));
    setSplitterStyle(_splitterStyle);
    static void const* const DelegateKey = &DelegateKey;
    [view setDelegate:gSplitViewDelegate];
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
    Position leftPosition = left->position();
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
        Rect childFrame = { Position(axis, offset), frame.size() };
        childFrame.size()[axis] = childSize;
        if (axis == Axis::Y) {
            // We need to do this coordinate transform dance because here AppKit
            // uses top-left -> bottom-right coordinates, unlike everywhere else
            childFrame.pos().y =
                frame.height() - childFrame.height() - childFrame.pos().y;
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
    View(PrivateViewKey, { LayoutMode::Flex, LayoutMode::Flex }),
    elements(std::move(elems)) {
    NSTabView* view = [[EventTabView alloc] init];
    view.tabPosition = toNS(_tabPosition);
    view.tabViewBorderType = toNS(_border);
    setNativeHandle(retain(view));
    for (auto& [title, child]: elements) {
        child->_parent = this;
        NSTabViewItem* item = [[NSTabViewItem alloc] init];
        item.view = transfer(child->nativeHandle());
        item.label = toNSString(title);
        [view addTabViewItem:item];
    }
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

ButtonView::ButtonView(std::string label, std::function<void()> action,
                       ButtonType type):
    View(PrivateViewKey, { LayoutMode::Static, LayoutMode::Static },
         MinSize({ 80, 34 })),
    _type(type),
    _label(std::move(label)),
    _action(std::move(action)) {
    NSButton* button = [[EventButtonView alloc] init];
    button.buttonType = toNS(type);
    setNativeHandle(retain(button));
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

SwitchView::SwitchView():
    View(PrivateViewKey, { LayoutMode::Static, LayoutMode::Static }) {
    NSSwitch* view = [[EventSwitchView alloc] init];
    setNativeHandle(retain(view));
    setMinSize(fromNSSize(view.intrinsicContentSize));
}

// MARK: - TextField

EVENT_VIEW_SUBCLASS(EventTextField, NSTextField)

TextFieldView::TextFieldView(std::string defaultText):
    View(PrivateViewKey, { LayoutMode::Flex, LayoutMode::Static },
         MinSize({ 80, 32 })) {
    NSTextField* view =
        [EventTextField textFieldWithString:toNSString(defaultText)];
    view.bezelStyle = NSTextFieldRoundedBezel;
    setAttribute<ViewAttributeKey::PaddingX>(6);
    setAttribute<ViewAttributeKey::PaddingY>(6);
    setNativeHandle(retain(view));
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

LabelView::LabelView(std::string text):
    View(PrivateViewKey, { LayoutMode::Flex, LayoutMode::Static },
         MinSize({ 80, 22 })) {
    NSTextField* field = [EventTextField labelWithString:toNSString(text)];
    setNativeHandle(retain(field));
}

void LabelView::setText(std::string text) {
    NSTextField* field = transfer(nativeHandle());
    field.stringValue = toNSString(text);
}

// MARK: - ProgressIndicatorView

EVENT_VIEW_SUBCLASS(EventProgressIndicator, NSProgressIndicator)

ProgressIndicatorView::ProgressIndicatorView(Style style):
    View(PrivateViewKey, { style == Bar ? LayoutMode::Flex : LayoutMode::Static,
                           LayoutMode::Static }) {
    NSProgressIndicator* view = [[EventProgressIndicator alloc] init];
    switch (style) {
    case Bar:
        view.style = NSProgressIndicatorStyleBar;
        setMinSize({ 0, 10 });
        break;
    case Spinner:
        view.style = NSProgressIndicatorStyleSpinning;
        setMinSize({ 20, 20 });
        break;
    }
    [view startAnimation:nil];
    setNativeHandle(retain(view));
}

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

ColorView::ColorView(Color const& color):
    View(PrivateViewKey, { LayoutMode::Flex, LayoutMode::Flex }) {
    FlatColorView* view =
        [[FlatColorView alloc] initWithColor:toNSColor(color)];
    setNativeHandle(retain(view));
}

void ColorView::doLayout(Rect frame) { setFrame(frame); }

// MARK: - Custom view

struct View::CustomImpl {
    static void draw(View& view, Rect frame) { view.draw(frame); }
};

@interface AetherCustomView: EventView
@end
@implementation AetherCustomView

- (void)drawRect:(NSRect)dirtyRect {
    View::CustomImpl::draw(*getView(self),
                           fromAppkitCoords(dirtyRect, self.frame.size.height));
    CGFloat cornerRadius = 10.0;
    NSRect rect = NSInsetRect({ {}, self.bounds.size }, 2.0, 2.0);
    NSBezierPath* roundedRect = [NSBezierPath
        bezierPathWithRoundedRect:rect
                          xRadius:cornerRadius
                          yRadius:cornerRadius];
    [[NSColor blackColor] setStroke];
    [roundedRect setLineWidth:2.0];
    [roundedRect stroke];
}

- (BOOL)clipsToBounds {
    return YES;
}
@end

View::View(Vec2<LayoutMode> layoutMode, detail::MinSize minSize,
           detail::PrefSize prefSize, detail::MaxSize maxSize):
    View(PrivateViewKey, layoutMode, minSize, prefSize, maxSize) {
    AetherCustomView* native = [[AetherCustomView alloc] init];
    setNativeHandle(retain(native));
}
