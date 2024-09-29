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
