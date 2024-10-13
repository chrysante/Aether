#include "Aether/MacOS/MacOSUtil.h"

#import <Appkit/Appkit.h>

using namespace xui;

static double getScreenHeight() {
    NSScreen* screen = [NSScreen mainScreen];
    return [screen frame].size.height;
}

NSPoint xui::toAppkitCoords(xui::Position pos) {
    return toAppkitCoords(pos, getScreenHeight());
}

xui::Position xui::fromAppkitCoords(NSPoint pos) {
    return fromAppkitCoords(pos, getScreenHeight());
}

xui::Rect xui::fromAppkitCoords(NSRect rect) {
    return fromAppkitCoords(rect, getScreenHeight());
}

xui::Rect xui::fromAppkitCoords(NSRect rect, double height) {
    Position pos = { rect.origin.x, height - rect.origin.y - rect.size.height };
    return { pos, fromNSSize(rect.size) };
}

NSString* xui::toNSString(std::string const& str) {
    return toNSString(str.c_str());
}

NSString* xui::toNSString(char const* str) {
    return
        [[NSString alloc] initWithCString:str
                                 encoding:NSStringEncodingConversionAllowLossy];
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
