#include <stdlib.h>

#import <Cocoa/Cocoa.h>

#include "Aether/Application.h"

static std::unique_ptr<xui::Application> gApp;

@interface AppDelegate: NSObject <NSApplicationDelegate>

@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification*)aNotification {
    gApp = xui::createApplication();
    [[NSApplication sharedApplication] activateIgnoringOtherApps:YES];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
    return YES;
}

- (void)applicationWillTerminate:(NSNotification*)aNotification {
    gApp.reset();
}

@end

int macOSMain(int argc, char const** argv) {
    @autoreleasepool {
        // Create an instance of NSApplication
        NSApplication* NSApp = [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        // Set the delegate to your custom AppDelegate
        AppDelegate* delegate = [[AppDelegate alloc] init];
        [NSApp setDelegate:delegate];

        // Set the main menu programmatically if needed
        NSMenu* menuBar = [[NSMenu alloc] init];
        NSMenuItem* appMenuItem = [[NSMenuItem alloc] init];
        [menuBar addItem:appMenuItem];
        [NSApp setMainMenu:menuBar];

        NSMenu* appMenu = [[NSMenu alloc] init];
        NSString* quitTitle = [NSString
            stringWithFormat:@"Quit %@",
                             [[NSProcessInfo processInfo] processName]];
        NSMenuItem* quitMenuItem = [[NSMenuItem alloc]
            initWithTitle:quitTitle
                   action:@selector(terminate:)
            keyEquivalent:@"q"];
        [appMenu addItem:quitMenuItem];
        [appMenuItem setSubmenu:appMenu];
        [NSApp finishLaunching];

        // Custom event loop
        while (true) {
            NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                                untilDate:[NSDate distantFuture]
                                                   inMode:NSDefaultRunLoopMode
                                                  dequeue:YES];
            if (!event) {
                break;
            }
            [NSApp sendEvent:event];
            [NSApp updateWindows];
        }
    }
    return 0;
}
