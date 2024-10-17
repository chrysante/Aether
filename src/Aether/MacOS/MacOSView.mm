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
    E make(auto... args) {
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

struct View::Impl {
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

@interface EventHandler: NSView
@property MouseTrackingKind kind;
@property MouseTrackingActivity activity;
@property NSTrackingArea* trackingArea;
@end
@implementation EventHandler
#define EVENT_TYPE_IMPL(Name, AppkitName)                                      \
    -(void)AppkitName: (NSEvent*)event {                                       \
        if (!View::Impl::handleEvent(EventType::Name,                          \
                                     *getView(self.superview), event))         \
            [super AppkitName:event];                                          \
    }

EVENT_TYPE_IMPL(ScrollEvent, scrollWheel)
EVENT_TYPE_IMPL(MouseDownEvent, mouseDown)
EVENT_TYPE_IMPL(MouseUpEvent, mouseUp)
EVENT_TYPE_IMPL(MouseMoveEvent, mouseMoved)
EVENT_TYPE_IMPL(MouseDragEvent, mouseDragged)
EVENT_TYPE_IMPL(MouseEnterEvent, mouseEntered)
EVENT_TYPE_IMPL(MouseExitEvent, mouseExited)

- (void)setFrame:(NSRect)frame {
    bool eq = NSEqualRects(frame, self.frame);
    [super setFrame:frame];
    if (eq || !self.hasTrackingArea) {
        return;
    }
    [self updateTrackingArea];
}
- (NSTrackingArea*)makeTrackingArea {
    NSUInteger options = 0;
    if (test(self.kind & MouseTrackingKind::Transition)) {
        options |= NSTrackingMouseEnteredAndExited;
    }
    if (test(self.kind & MouseTrackingKind::Movement)) {
        options |= NSTrackingMouseMoved;
    }
    switch (self.activity) {
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
    return [[NSTrackingArea alloc] initWithRect:self.bounds
                                        options:options
                                          owner:self
                                       userInfo:nil];
}
- (void)updateTrackingArea {
    [self removeTrackingArea:self.trackingArea];
    self.trackingArea = [self makeTrackingArea];
    [self addTrackingArea:self.trackingArea];
}
- (BOOL)hasTrackingArea {
    return !!self.trackingArea;
}
- (void)setHasTrackingArea:(BOOL)value {
    if (value) {
        if (!self.trackingArea) {
            self.trackingArea = [self makeTrackingArea];
        }
    }
    else {
        [self removeTrackingArea:self.trackingArea];
        self.trackingArea = nil;
    }
}

@end

static void const* const EventHandlerKey = &EventHandlerKey;

static EventHandler* getEventHandler(NSView* native) {
    return objc_getAssociatedObject(native, EventHandlerKey);
}

static EventHandler* getOrInstallEventHandler(NSView* native) {
    if (EventHandler* handler = getEventHandler(native)) {
        return handler;
    }
    EventHandler* handler = [[EventHandler alloc] init];
    [native addSubview:handler];
    objc_setAssociatedObject(native, EventHandlerKey, handler,
                             OBJC_ASSOCIATION_RETAIN);
    return handler;
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
    (void)getOrInstallEventHandler(transfer(nativeHandle()));
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

static MouseTrackingActivity max(MouseTrackingActivity a,
                                 MouseTrackingActivity b) {
    return (MouseTrackingActivity)std::max((int)a, (int)b);
}

void View::trackMouseMovement(MouseTrackingKind kind,
                              MouseTrackingActivity activity) {
    EventHandler* eventHandler =
        getOrInstallEventHandler(transfer(nativeHandle()));
    eventHandler.kind |= kind;
    eventHandler.activity = ::max(eventHandler.activity, activity);
    [eventHandler updateTrackingArea];
}

bool View::setFrame(Rect frame) {
    NSView* view = transfer(nativeHandle());
    NSRect newFrame = toAppkitCoords(frame, view.superview.frame.size.height);
    bool eq = NSEqualRects(view.frame, newFrame);
    if (!eq) {
        view.frame = newFrame;
    }
    if (EventHandler* eventHandler = getEventHandler(view)) {
        eventHandler.frame = { {}, newFrame.size };
    }
    return !eq;
}

// MARK: - StackView

StackView::StackView(Axis axis, std::vector<std::unique_ptr<View>> children):
    AggregateView(std::move(children), { LayoutMode::Flex, LayoutMode::Flex }),
    axis(axis) {
    NSView* view = [[NSView alloc] init];
    for (auto& child: _children) {
        NSView* childView = transfer(child->nativeHandle());
        [view addSubview:childView];
    }
    setNativeHandle(retain(view));
}

// MARK: - ScrollView

ScrollView::ScrollView(Axis axis, std::vector<std::unique_ptr<View>> children):
    AggregateView(std::move(children), { LayoutMode::Flex, LayoutMode::Flex }),
    axis(axis) {
    NSView* content = [[NSView alloc] init];
    NSScrollView* VScrollView = [[NSScrollView alloc] init];
    for (auto& child: _children) {
        NSView* childView = transfer(child->nativeHandle());
        [content addSubview:childView];
    }
    [VScrollView setDocumentView:content];
    VScrollView.hasHorizontalScroller = axis == Axis::X;
    VScrollView.hasVerticalScroller = axis == Axis::Y;
    setNativeHandle(retain(VScrollView));
}

void ScrollView::setDocumentSize(Size size) {
    NSScrollView* view = transfer(nativeHandle());
    view.documentView.frame = { {}, toNSSize(size) };
}

// MARK: - SplitView

@interface AetherSplitView: NSSplitView
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
    AggregateView(std::move(children), { LayoutMode::Flex, LayoutMode::Flex }),
    axis(axis) {
    AetherSplitView* view = [[AetherSplitView alloc] init];
    view.This = this;
    view.vertical = axis == Axis::X;
    for (auto& child: _children) {
        [view addSubview:transfer(child->nativeHandle())];
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
    return totalSize - view.dividerThickness * (_children.size() - 1);
}

bool SplitView::isChildCollapsed(size_t childIndex) const {
    NSSplitView* view = (NSSplitView*)transfer(nativeHandle());
    NSView* subview = transfer(_children[childIndex]->nativeHandle());
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
    assert(childFractions.size() == view.subviews.count);
    auto frame = fromAppkitCoords(view.frame, view.superview.frame.size.height);
    double totalSize = sizeWithoutDividers();
    double fracSum = 0;
    for (size_t i = 0; i < _children.size(); ++i) {
        auto* child = _children[i].get();
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
    auto* left = _children[index].get();
    auto* right = _children[index + 1].get();
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
        double frac = 1.0 / _children.size();
        childFractions.resize(_children.size(), frac);
    }
    double offset = 0;
    for (size_t i = 0; i < _children.size(); ++i) {
        if (isChildCollapsed(i)) {
            offset += dividerThickness;
            continue;
        }
        auto& child = _children[i];
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

TabView::TabView(std::vector<TabViewElement> elems):
    View({ LayoutMode::Flex, LayoutMode::Flex }), elements(std::move(elems)) {
    NSTabView* view = [[NSTabView alloc] init];
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

ButtonView::ButtonView(std::string label, std::function<void()> action,
                       ButtonType type):
    View({ LayoutMode::Static, LayoutMode::Static }, MinSize({ 80, 34 })),
    _type(type),
    _label(std::move(label)),
    _action(std::move(action)) {
    NSButton* button = [[NSButton alloc] init];
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

SwitchView::SwitchView(): View({ LayoutMode::Static, LayoutMode::Static }) {
    NSSwitch* view = [[NSSwitch alloc] init];
    setNativeHandle(retain(view));
    setMinSize(fromNSSize(view.intrinsicContentSize));
}

// MARK: - TextField

TextFieldView::TextFieldView(std::string defaultText):
    View({ LayoutMode::Flex, LayoutMode::Static }, MinSize({ 80, 32 })) {
    NSTextField* view =
        [NSTextField textFieldWithString:toNSString(defaultText)];
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
    View({ LayoutMode::Flex, LayoutMode::Static }, MinSize({ 80, 22 })) {
    NSTextField* field = [NSTextField labelWithString:toNSString(text)];
    setNativeHandle(retain(field));
}

void LabelView::setText(std::string text) {
    NSTextField* field = transfer(nativeHandle());
    field.stringValue = toNSString(text);
}

// MARK: - ProgressIndicatorView

ProgressIndicatorView::ProgressIndicatorView(Style style):
    View({ style == Bar ? LayoutMode::Flex : LayoutMode::Static,
           LayoutMode::Static }) {
    NSProgressIndicator* view = [[NSProgressIndicator alloc] init];
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

@interface FlatColorView: NSView
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
    [super drawRect:dirtyRect];
    [[NSColor blackColor] setFill];
    NSRectFill(dirtyRect);
    [self.color setFill];
    NSRectFill(NSInsetRect(dirtyRect, 2, 2));
    NSString* text = [NSString stringWithFormat:@"Width: %f\nHeight: %f",
                                                self.frame.size.width,
                                                self.frame.size.height];
    NSDictionary* attributes = @{
        NSFontAttributeName : [NSFont systemFontOfSize:12],
        NSForegroundColorAttributeName : [NSColor blackColor]
    };
    NSRect textRect = NSInsetRect(dirtyRect, 15, 15);
    [text drawInRect:textRect withAttributes:attributes];
}
@end

ColorView::ColorView(Color const& color):
    View({ LayoutMode::Flex, LayoutMode::Flex }) {
    FlatColorView* view =
        [[FlatColorView alloc] initWithColor:toNSColor(color)];
    setNativeHandle(retain(view));
}

void ColorView::doLayout(Rect frame) { setFrame(frame); }
