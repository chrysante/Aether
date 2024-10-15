#define AETHER_VIEW_IMPL

#include "Aether/View.h"

#include <functional>
#include <optional>
#include <span>

#import <Appkit/Appkit.h>
#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>

#include "Aether/MacOS/MacOSUtil.h"
#include "Aether/ViewUtil.h"

using namespace xui;

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

static void* NativeHandleKey = &NativeHandleKey;

void View::setNativeHandle(void* handle) {
    _nativeHandle = handle;
    objc_setAssociatedObject(transfer(handle), NativeHandleKey,
                             (__bridge id)this, OBJC_ASSOCIATION_ASSIGN);
}

static View* getView(NSView* view) {
    return (View*)(__bridge void*)objc_getAssociatedObject(view,
                                                           NativeHandleKey);
}

template <typename V>
static V* getView(NSView* view) {
    return dynamic_cast<V*>(getView(view));
}

// MARK: - StackView

StackView::StackView(Axis axis, std::vector<std::unique_ptr<View>> children):
    AggregateView(axis, std::move(children)) {
    _layoutMode = LayoutMode::Flex;
    NSView* view = [[NSView alloc] init];
    for (auto& child: _children) {
        NSView* childView = transfer(child->nativeHandle());
        [view addSubview:childView];
    }
    setNativeHandle(retain(view));
}

void StackView::setFrame(Rect frame) {
    NSView* view = transfer(nativeHandle());
    view.frame = toAppkitCoords(frame, view.superview.frame.size.height);
}

// MARK: - ScrollView

ScrollView::ScrollView(Axis axis, std::vector<std::unique_ptr<View>> children):
    AggregateView(axis, std::move(children)) {
    _layoutMode = LayoutMode::Flex;
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

void ScrollView::setFrame(Rect frame) {
    NSScrollView* view = transfer(nativeHandle());
    view.frame = toAppkitCoords(frame, view.superview.frame.size.height);
    view.documentView.frame = { {}, view.frame.size };
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
    AggregateView(axis, std::move(children)) {
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
    double totalSize = fromNSSize(view.frame.size)[axis()];
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
        double size = child->size()[axis()];
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
    double currentPosition = leftPosition[axis()] + left->size()[axis()];
    if (left->size()[axis()] <= left->minSize()[axis()] &&
        right->size()[axis()] <= right->minSize()[axis()])
    {
        return currentPosition;
    }
    double offset = proposedPosition - currentPosition;
    double leftNewSize = left->size()[axis()] + offset;
    if (left->minSize()[axis()] > leftNewSize) {
        return leftPosition[axis()] + left->minSize()[axis()];
    }
    double rightNewSize = right->size()[axis()] - offset;
    if (right->minSize()[axis()] > rightNewSize) {
        return currentPosition + right->size()[axis()] -
               right->minSize()[axis()];
    }
    return proposedPosition;
}

void SplitView::doLayout(Rect frame) {
    NSSplitView* view = transfer(nativeHandle());
    double dividerThickness = view.dividerThickness;
    auto newFrame = toAppkitCoords(frame, view.superview.frame.size.height);
    if (!NSEqualRects(view.frame, newFrame)) {
        view.frame = newFrame;
        // We return because [view setFrame:] recurses into us
        if (!childFractions.empty()) {
            return;
        }
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
        Rect childFrame = { Position(axis(), offset), frame.size() };
        childFrame.size()[axis()] = childSize;
        if (axis() == Axis::Y) {
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
    elements(std::move(elems)) {
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
    NSTabView* view = transfer(nativeHandle());
    view.frame = toAppkitCoords(frame, view.superview.frame.size.height);
    if (elements.empty()) {
        return;
    }
    NSView* selected = view.selectedTabViewItem.view;
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

ButtonView::ButtonView(std::string label, std::function<void()> action):
    _label(std::move(label)), _action(std::move(action)) {
    NSButton* button = [[NSButton alloc] init];
    setNativeHandle(retain(button));
    [button setTitle:toNSString(_label)];
    button.enabled = true;
    [button setActionBlock:^{
        if (_action) {
            _action();
        }
    }];
    _minSize = { 80, 34 };
    _preferredSize = fromNSSize(button.intrinsicContentSize);
}

void ButtonView::doLayout(Rect rect) {
    NSButton* button = transfer(nativeHandle());
    button.frame = toAppkitCoords(rect, button.superview.frame.size.height);
}

// MARK: - Switch

SwitchView::SwitchView() {
    NSSwitch* view = [[NSSwitch alloc] init];
    setNativeHandle(retain(view));
    _minSize = _preferredSize = fromNSSize(view.intrinsicContentSize);
}

void SwitchView::doLayout(Rect frame) {
    NSSwitch* view = transfer(nativeHandle());
    view.frame = toAppkitCoords(frame, view.superview.frame.size.height);
}

// MARK: - TextField

TextFieldView::TextFieldView(std::string defaultText) {
    NSTextField* view =
        [NSTextField textFieldWithString:toNSString(defaultText)];
    view.bezelStyle = NSTextFieldRoundedBezel;
    setAttribute<ViewAttributeKey::PaddingX>(6);
    setAttribute<ViewAttributeKey::PaddingY>(6);
    setNativeHandle(retain(view));
    _minSize = _preferredSize = { 80, 34 };
    _layoutMode = { LayoutMode::Flex, LayoutMode::Static };
}

void TextFieldView::setText(std::string /* text */) {
    NSTextField* field = transfer(nativeHandle());
    assert(field && false);
}

std::string TextFieldView::getText() const {
    NSTextField* field = transfer(nativeHandle());
    return toStdString([field stringValue]);
}

void TextFieldView::doLayout(Rect frame) {
    NSView* view = transfer(nativeHandle());
    view.frame = toAppkitCoords(frame, view.superview.frame.size.height);
}

// MARK: - LabelView

LabelView::LabelView(std::string text) {
    NSTextField* field = [NSTextField labelWithString:toNSString(text)];
    setNativeHandle(retain(field));
    _minSize = _preferredSize = { 80, 22 };
}

void LabelView::doLayout(Rect frame) {
    NSTextField* field = transfer(nativeHandle());
    field.frame = toAppkitCoords(frame, field.superview.frame.size.height);
}

// MARK: - ProgressIndicatorView

ProgressIndicatorView::ProgressIndicatorView(Style style) {
    NSProgressIndicator* view = [[NSProgressIndicator alloc] init];
    switch (style) {
    case Bar:
        view.style = NSProgressIndicatorStyleBar;
        _minSize = { 0, 10 };
        break;
    case Spinner:
        view.style = NSProgressIndicatorStyleSpinning;
        _minSize = { 20, 20 };
        break;
    }
    [view startAnimation:nil];
    setNativeHandle(retain(view));
}

void ProgressIndicatorView::doLayout(Rect frame) {
    NSProgressIndicator* view = (NSProgressIndicator*)transfer(nativeHandle());
    view.frame = toAppkitCoords(frame, view.superview.frame.size.height);
}

// MARK: - ColorView

@interface FlatColorView: NSView
@property(nonatomic, strong) NSColor* color;
@end
@implementation FlatColorView
- (instancetype)initWithColor:(NSColor*)color {
    self = [super init];
    if (self) {
        self.color = color;
    }
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

ColorView::ColorView(Color const& color) {
    FlatColorView* view =
        [[FlatColorView alloc] initWithColor:toNSColor(color)];
    setNativeHandle(retain(view));
}

void ColorView::doLayout(Rect frame) {
    NSView* view = transfer(nativeHandle());
    view.frame = toAppkitCoords(frame, view.superview.frame.size.height);
}
