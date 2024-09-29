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
