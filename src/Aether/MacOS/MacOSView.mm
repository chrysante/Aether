#include "Aether/View.h"

#import <Appkit/Appkit.h>
#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>

#include "Aether/MacOS/MacOSUtil.h"

using namespace xui;

View::~View() { release(_handle); }

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

struct SplitView::Impl {
    static void handleResize(SplitView* splitView,
                             NSSplitView __weak* nsSplitView) {
        if (splitView->childFractions.empty()) {
            return;
        }
        assert(splitView->childFractions.size() == nsSplitView.subviews.count);
        auto frame = fromAppkitCoords(nsSplitView.frame,
                                      nsSplitView.superview.frame.size.height);
        double totalWidth = frame.size.width();
        for (size_t i = 0; i < nsSplitView.subviews.count; ++i) {
            NSRect childFrame = nsSplitView.subviews[i].frame;
            splitView->childFractions[i] = childFrame.size.width / totalWidth;
        }
        splitView->layout(frame);
    }
};

@interface SplitViewDelegate: NSObject <NSSplitViewDelegate>
@property SplitView* splitView;
@property(weak) NSSplitView* nsSplitView;
@end
@implementation SplitViewDelegate

- (void)splitViewWillResizeSubviews:(NSNotification*)notification {
    SplitView::Impl::handleResize(self.splitView, self.nsSplitView);
}

@end

SplitView::SplitView(Axis axis, std::vector<std::unique_ptr<View>> children):
    AggregateView(axis, std::move(children)) {
    NSSplitView* view = [[NSSplitView alloc] init];
    view.vertical = YES;
    view.arrangesAllSubviews = YES;
    for (auto& child: _children) {
        [view addSubview:transfer(child->nativeHandle())];
    }
    _handle = retain(view);

    static void const* const DelegateKey = &DelegateKey;
    SplitViewDelegate* delegate = [[SplitViewDelegate alloc] init];
    delegate.splitView = this;
    delegate.nsSplitView = view;
    objc_setAssociatedObject(view, DelegateKey, delegate,
                             OBJC_ASSOCIATION_RETAIN);
    [view setDelegate:delegate];
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
    if (childFractions.empty()) {
        double frac =
            ((frame.size.width() - dividerThickness * _children.size()) /
             frame.size.width()) /
            _children.size();
        childFractions.resize(_children.size(), frac);
    }
    double totalWidth = frame.size.width();
    double totalHeight = frame.size.height();
    double leftOffset = 0;
    for (size_t i = 0; i < _children.size(); ++i) {
        auto& child = _children[i];
        double frac = childFractions[i];
        assert(frac >= 0.0);
        double childWidth = totalWidth * frac;
        Rect childFrame = { { leftOffset, 0 }, { childWidth, totalHeight } };
        child->layout(childFrame);
        leftOffset += childWidth + dividerThickness;
    }
}

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

void TextFieldView::setText(std::string text) {
    NSTextField* field = transfer(_handle);
    assert(false);
}

std::string TextFieldView::getText() const {
    NSTextField* field = [transfer(_handle) wrapped];
    return toStdString([field stringValue]);
}

void TextFieldView::doLayout(Rect frame) {
    NSView* view = transfer(_handle);
    view.frame = toAppkitCoords(frame, view.superview.frame.size.height);
}

LabelView::LabelView(std::string text) {
    NSTextField* field = [NSTextField labelWithString:toNSString(text)];
    _handle = retain(field);
    _minSize = _preferredSize = { 80, 22 };
}

void LabelView::doLayout(Rect frame) {
    NSTextField* field = transfer(_handle);
    field.frame = toAppkitCoords(frame, field.superview.frame.size.height);
}

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
    NSRectFill(NSInsetRect(dirtyRect, 10, 10));
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
