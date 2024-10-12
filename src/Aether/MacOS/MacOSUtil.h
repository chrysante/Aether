#ifndef AETHER_MACOS_MACOSUTIL_H
#define AETHER_MACOS_MACOSUTIL_H

#include <string>

#import <Appkit/NSColor.h>
#import <Foundation/Foundation.h>

#include "Aether/ADT.h"

namespace xui {

inline void* retain(id obj) { return (__bridge_retained void*)obj; }

inline id release(void* obj) { return (__bridge_transfer id)obj; }

inline id transfer(void* obj) { return (__bridge id)obj; }

inline NSSize toNSSize(xui::Size size) {
    return { size.width(), size.height() };
}

inline xui::Size fromNSSize(NSSize size) { return { size.width, size.height }; }

inline NSRect toNSRect(xui::Rect rect) {
    return NSMakeRect(rect.pos.x, rect.pos.y, rect.size.width(),
                      rect.size.height());
}

NSPoint toAppkitCoords(xui::Position pos);

inline NSPoint toAppkitCoords(xui::Position pos, double height) {
    return { pos.x, height - pos.y };
}

inline NSRect toAppkitCoords(xui::Rect rect) {
    auto pos = toAppkitCoords(rect.pos);
    pos.y -= rect.size.height();
    return { pos, NSSize{ rect.size.width(), rect.size.height() } };
}

inline NSRect toAppkitCoords(xui::Rect rect, double height) {
    auto pos = toAppkitCoords(rect.pos, height);
    pos.y -= rect.size.height();
    return { pos, NSSize{ rect.size.width(), rect.size.height() } };
}

Position fromAppkitCoords(NSPoint pos);

inline Position fromAppkitCoords(NSPoint pos, double height) {
    return { pos.x, height - pos.y };
}

xui::Rect fromAppkitCoords(NSRect rect);

xui::Rect fromAppkitCoords(NSRect rect, double height);

inline xui::Rect fromNSRect(NSRect rect) {
    return { { rect.origin.x, rect.origin.y },
             { rect.size.width, rect.size.height } };
}

NSString* toNSString(std::string const& str);

NSString* toNSString(char const* str);

std::string toStdString(NSString* str);

NSColor* toNSColor(Color const& color);

Color fromNSColor(NSColor* nsColor);

} // namespace xui

#endif // AETHER_MACOS_MACOSUTIL_H
