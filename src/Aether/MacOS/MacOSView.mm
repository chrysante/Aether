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
    view.frame =
        toAppkitCoords(frame, view.window.contentView.frame.size.height);
}

ScrollView::ScrollView(Axis axis, std::vector<std::unique_ptr<View>> children):
    AggregateView(axis, std::move(children)) {
    _layoutMode = LayoutMode::Flex;
    NSView* content = [[NSView alloc] init];
    NSScrollView* scrollView = [[NSScrollView alloc] init];
    for (auto& child: _children) {
        NSView* childView = transfer(child->nativeHandle());
        [content addSubview:childView];
    }
    [scrollView setDocumentView:content];
    switch (axis) {
    case Axis::X:
        scrollView.hasHorizontalScroller = YES;
        break;
    case Axis::Y:
        scrollView.hasVerticalScroller = YES;
        break;
    default:
        break;
    }
    scrollView.hasVerticalScroller = YES;
    _handle = retain(scrollView);
}

void ScrollView::setFrame(Rect frame) {
    NSScrollView* view = transfer(_handle);
    view.frame =
        toAppkitCoords(frame, view.window.contentView.frame.size.height);
    view.documentView.frame = { {}, view.frame.size };
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

Button::Button(std::string label, std::function<void()> action):
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

void Button::doLayout(Rect rect) {
    NSButton* button = transfer(_handle);
    button.frame =
        toAppkitCoords(rect, button.window.contentView.frame.size.height);
    int i;
    (void)i;
}

TextField::TextField(std::string defaultText) {
    NSTextField* field =
        [NSTextField textFieldWithString:toNSString(defaultText)];
    _handle = retain(field);
}

void TextField::doLayout(Rect frame) {
    NSTextField* field = transfer(_handle);
    field.frame =
        toAppkitCoords(frame, field.window.contentView.frame.size.height);
}
