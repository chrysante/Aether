#define AETHER_VIEW_IMPL

#include "Aether/View.h"

#include <functional>
#include <optional>

#import <Appkit/Appkit.h>
#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>

#include "Aether/MacOS/MacOSUtil.h"
#include "Aether/ViewUtil.h"

using namespace xui;

View::~View() { release(_handle); }

Position View::position() const {
    NSView* view = transfer(_handle);
    auto pos =
        fromAppkitCoords(view.frame.origin, view.superview.frame.size.height);
    pos.y -= view.frame.size.height;
    return pos;
}

xui::Size View::size() const {
    NSView* view = transfer(_handle);
    return fromNSSize(view.frame.size);
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
    _handle = retain(view);
}

void StackView::setFrame(Rect frame) {
    NSView* view = transfer(_handle);
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
    _handle = retain(VScrollView);
}

void ScrollView::setFrame(Rect frame) {
    NSScrollView* view = transfer(_handle);
    view.frame = toAppkitCoords(frame, view.superview.frame.size.height);
    view.documentView.frame = { {}, view.frame.size };
}

void ScrollView::setDocumentSize(Size size) {
    NSScrollView* view = transfer(_handle);
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
    _handle = retain(view);
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
    NSSplitView* view = transfer(_handle);
    view.dividerStyle = toNS(style);
}

void SplitView::setSplitterColor(std::optional<Color> color) {
    _splitterColor = color;
    AetherSplitView* view = transfer(_handle);
    view.divColor = toNSColor(color);
}

void SplitView::setSplitterThickness(std::optional<double> thickness) {
    _splitterThickness = thickness;
    AetherSplitView* view = transfer(_handle);
    view.divThickness = thickness;
}

double SplitView::sizeWithoutDividers() const {
    NSSplitView* view = transfer(_handle);
    return fromNSSize(view.frame.size)[(size_t)axis()] -
           view.dividerThickness * (_children.size() - 1);
}

void SplitView::didResizeSubviews() {
    AetherSplitView* view = (AetherSplitView*)transfer(_handle);
    if (childFractions.empty()) {
        return;
    }
    assert(childFractions.size() == view.subviews.count);
    auto frame = fromAppkitCoords(view.frame, view.superview.frame.size.height);
    double totalSize = sizeWithoutDividers();
    double fracSum = 0;
    for (size_t i = 0; i < _children.size(); ++i) {
        auto* child = _children[i].get();
        double size = child->size()[(size_t)axis()];
        double frac = size / totalSize;
        childFractions[i] = frac;
        fracSum += frac;
    }
    for (auto& frac: childFractions) {
        frac /= fracSum;
    }
    layout(frame);
}

double SplitView::constrainSplitPosition(double proposedPosition,
                                         size_t index) const {
    size_t const AI = (size_t)axis();
    auto* left = _children[index].get();
    auto* right = _children[index + 1].get();
    // Must transform the position again because of the flipped coordinate
    // system
    Position leftPosition = left->position();
    leftPosition.y += left->size().height();
    leftPosition.y = size().height() - leftPosition.y;
    double currentPosition = leftPosition[AI] + left->size()[AI];
    double offset = proposedPosition - currentPosition;
    double leftNewSize = left->size()[AI] + offset;
    double rightNewSize = right->size()[AI] - offset;
    bool leftViolated = left->minSize()[AI] > leftNewSize;
    bool rightViolated = right->minSize()[AI] > rightNewSize;
    if (leftViolated && rightViolated) {
        return currentPosition;
    }
    if (leftViolated) {
        return leftPosition[AI] + left->minSize()[AI];
    }
    if (rightViolated) {
        return currentPosition + right->size()[AI] - right->minSize()[AI];
    }
    return proposedPosition;
}

void SplitView::doLayout(Rect frame) {
    NSSplitView* view = transfer(_handle);
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
        auto& child = _children[i];
        double frac = childFractions[i];
        assert(frac >= 0.0);
        double childSize = totalSize * frac;
        Rect childFrame = { Position(axis(), offset), frame.size() };
        childFrame.size()[(size_t)axis()] = childSize;
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
    _handle = retain(button);
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
    NSButton* button = transfer(_handle);
    button.frame = toAppkitCoords(rect, button.superview.frame.size.height);
    int i;
    (void)i;
}

// MARK: - TextField

@interface PaddedView: NSView
@property(nonatomic) id wrapped;
@property(nonatomic) double xPadding;
@property(nonatomic) double yPadding;
@end

@implementation PaddedView

- (instancetype)initWithView:(NSView*)view {
    self = [super init];
    self.wrapped = view;
    [self addSubview:view];
    return self;
}

- (void)setFrame:(NSRect)newFrame {
    NSRect inset = CGRectMake(self.xPadding, self.yPadding,
                              newFrame.size.width - 2 * self.xPadding,
                              newFrame.size.height - 2 * self.yPadding);
    [super setFrame:newFrame];
    [self.wrapped setFrame:inset];
}

@end

TextFieldView::TextFieldView(std::string defaultText) {
    NSTextField* field =
        [NSTextField textFieldWithString:toNSString(defaultText)];
    field.bezelStyle = NSTextFieldRoundedBezel;
    PaddedView* view = [[PaddedView alloc] initWithView:field];
    view.xPadding = 6;
    view.yPadding = 6;
    _handle = retain(view);
    _minSize = _preferredSize = { 80, 34 };
    _layoutMode = { LayoutMode::Flex, LayoutMode::Static };
}

void TextFieldView::setText(std::string /* text */) {
    NSTextField* field = transfer(_handle);
    assert(field && false);
}

std::string TextFieldView::getText() const {
    NSTextField* field = [transfer(_handle) wrapped];
    return toStdString([field stringValue]);
}

void TextFieldView::doLayout(Rect frame) {
    NSView* view = transfer(_handle);
    view.frame = toAppkitCoords(frame, view.superview.frame.size.height);
}

// MARK: - LabelView

LabelView::LabelView(std::string text) {
    NSTextField* field = [NSTextField labelWithString:toNSString(text)];
    _handle = retain(field);
    _minSize = _preferredSize = { 80, 22 };
}

void LabelView::doLayout(Rect frame) {
    NSTextField* field = transfer(_handle);
    field.frame = toAppkitCoords(frame, field.superview.frame.size.height);
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
    _handle = retain(view);
}

void ColorView::doLayout(Rect frame) {
    NSView* view = transfer(_handle);
    view.frame = toAppkitCoords(frame, view.superview.frame.size.height);
}
