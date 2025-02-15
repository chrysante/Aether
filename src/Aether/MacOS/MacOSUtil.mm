#include "Aether/MacOS/MacOSUtil.h"

#import <Appkit/Appkit.h>
#import <objc/runtime.h>

using namespace xui;

static double getScreenHeight() {
    NSScreen* screen = [NSScreen mainScreen];
    return [screen frame].size.height;
}

NSPoint xui::toAppkitCoords(xui::Point pos) {
    return toAppkitCoords(pos, getScreenHeight());
}

xui::Point xui::fromAppkitCoords(NSPoint pos) {
    return fromAppkitCoords(pos, getScreenHeight());
}

xui::Rect xui::fromAppkitCoords(NSRect rect) {
    return fromAppkitCoords(rect, getScreenHeight());
}

xui::Rect xui::fromAppkitCoords(NSRect rect, double height) {
    Point pos = { rect.origin.x, height - rect.origin.y - rect.size.height };
    return { pos, fromNSSize(rect.size) };
}

NSString* xui::toNSString(std::string_view str) {
    return [[NSString alloc] initWithBytes:str.data()
                                    length:str.size()
                                  encoding:NSUTF8StringEncoding];
}

std::string xui::toStdString(NSString* str) {
    return std::string(str.UTF8String);
}

NSColor* xui::toNSColor(Color const& c) {
    return [NSColor colorWithRed:c.red()
                           green:c.green()
                            blue:c.blue()
                           alpha:c.alpha()];
}

Color xui::fromNSColor(NSColor* nsColor) {
    NSColor* rgbColor =
        [nsColor colorUsingColorSpace:[NSColorSpace deviceRGBColorSpace]];
    if (!rgbColor) {
        return Color(0.0, 0.0, 0.0, 1.0);
    }
    return Color(rgbColor.redComponent, rgbColor.greenComponent,
                 rgbColor.blueComponent, rgbColor.alphaComponent);
}

void xui::setAssocPointer(id object, void const* key, void* pointer) {
    objc_setAssociatedObject(object, key, (__bridge id)pointer,
                             OBJC_ASSOCIATION_ASSIGN);
}

void* xui::getAssocPointer(id object, void const* key) {
    return (__bridge void*)objc_getAssociatedObject(object, key);
}
