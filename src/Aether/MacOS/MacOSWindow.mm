#include "Aether/Window.h"

#include <Appkit/Appkit.h>
#import <objc/runtime.h>

#include "Aether/MacOS/MacOSUtil.h"
#include "Aether/Toolbar.h"
#include "Aether/View.h"

using namespace xui;

struct internal::WindowImpl {
    static void layoutContent(Window& window) {
        if (!window._content) {
            return;
        }
        NSWindow* native = transfer(window._handle);
        NSRect contentRect = [NSWindow
            contentRectForFrameRect:native.frame
                          styleMask:native.styleMask];
        auto size = fromAppkitCoords(contentRect).size();
        window._content->layout({ { 0, 0 }, size });
    }

    static void onResize(Window& window, Rect newFrame) {
        layoutContent(window);
    }
};

using Impl = internal::WindowImpl;

@interface MyWindowDelegate: NSObject <NSWindowDelegate>
@property Window* window;
@end

@implementation MyWindowDelegate

- (BOOL)windowShouldClose:(NSWindow*)sender {
    // Return YES to allow the window to close, NO to prevent it
    return YES;
}

// Called when the window has been resized
- (void)windowDidResize:(NSNotification*)notification {
    NSWindow* window = [notification object];
    Impl::onResize(*self.window, fromNSRect(window.frame));
}

// Called when the window is about to become the key window
- (void)windowWillBecomeKey:(NSNotification*)notification {
}

// Called when the window did become the key window
- (void)windowDidBecomeKey:(NSNotification*)notification {
}

// Called when the window is about to resign the key status
- (void)windowWillResignKey:(NSNotification*)notification {
}

// Called when the window did resign the key status
- (void)windowDidResignKey:(NSNotification*)notification {
}

// Called when the window is about to move
- (void)windowWillMove:(NSNotification*)notification {
}

// Called when the window has moved
- (void)windowDidMove:(NSNotification*)notification {
}

// Called when the window is minimized
- (void)windowDidMiniaturize:(NSNotification*)notification {
}

// Called when the window is restored from minimized state
- (void)windowDidDeminiaturize:(NSNotification*)notification {
}

// Called before the window is ordered out (hidden)
- (void)windowWillClose:(NSNotification*)notification {
}

@end

static void const* DelegateKey = &DelegateKey;

Window::Window(std::string title, Rect frame, WindowProperties props,
               std::unique_ptr<View> content):
    _title(std::move(title)), _props(props) {
    NSWindowStyleMask styleMask = NSWindowStyleMaskTitled |
                                  NSWindowStyleMaskClosable |
                                  NSWindowStyleMaskResizable;
    NSRect contentRect = [NSWindow contentRectForFrameRect:toAppkitCoords(frame)
                                                 styleMask:styleMask];
    NSWindow* window = [[NSWindow alloc]
        initWithContentRect:contentRect
                  styleMask:styleMask
                    backing:NSBackingStoreBuffered
                      defer:NO];
    _handle = retain(window);
    MyWindowDelegate* delegate = [[MyWindowDelegate alloc] init];
    if (props.fullSizeContentView) {
        window.titlebarAppearsTransparent = YES;
        window.styleMask |= NSWindowStyleMaskFullSizeContentView;
    }
    delegate.window = this;
    objc_setAssociatedObject(window, DelegateKey, delegate,
                             OBJC_ASSOCIATION_RETAIN);
    [window setDelegate:delegate];
    setContentView(std::move(content));
    [window makeKeyAndOrderFront:nil];
}

Window::~Window() { release(_handle); }

void Window::setFrame(Rect frame, bool animate) {
    NSWindow* window = transfer(_handle);
    [window setFrame:toAppkitCoords(frame) display:YES animate:animate];
}

void Window::setTitle(std::string title) {
    NSWindow* native = transfer(_handle);
    _title = std::move(title);
    native.title = toNSString(_title);
}

void Window::setContentView(std::unique_ptr<View> view) {
    _content = std::move(view);
    NSWindow* native = transfer(_handle);
    if (_content) {
        [native.contentView
            setSubviews:@[ transfer(_content->nativeHandle()) ]];
        Impl::layoutContent(*this);
    }
}

void Window::setToolbar(std::unique_ptr<ToolbarView> toolbar) {
    _toolbar = std::move(toolbar);
    NSWindow* window = transfer(_handle);
    window.toolbarStyle = NSWindowToolbarStyleUnifiedCompact;
    window.toolbar = transfer(_toolbar->nativeHandle());
}

xui::Rect Window::frame() const {
    NSWindow* window = transfer(_handle);
    return fromAppkitCoords(window.frame);
}
