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

ButtonView::ButtonView(std::string label, std::function<void()> action):
    _label(std::move(label)), _action(std::move(action)) {
    _preferredSize = _minSize = { 80, 44 };
    NSButton* button = [[NSButton alloc] init];
    _handle = retain(button);
    [button setTitle:toNSString(_label)];
    button.enabled = true;
    [button setActionBlock:^{
        if (_action) {
            _action();
        }
    }];
}

void ButtonView::doLayout(Rect rect) {
    NSButton* button = transfer(_handle);
    button.frame = toAppkitCoords(rect, button.superview.frame.size.height);
    int i;
    (void)i;
}

TextFieldView::TextFieldView(std::string defaultText) {
    NSTextField* field =
        [NSTextField textFieldWithString:toNSString(defaultText)];
    _handle = retain(field);
    _minSize = _preferredSize = { 80, 22 };
    _layoutMode = { LayoutMode::Flex, LayoutMode::Static };
}

void TextFieldView::doLayout(Rect frame) {
    NSTextField* field = transfer(_handle);
    field.frame = toAppkitCoords(frame, field.superview.frame.size.height);
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
