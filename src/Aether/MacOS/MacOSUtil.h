#ifndef AETHER_MACOS_MACOSUTIL_H
#define AETHER_MACOS_MACOSUTIL_H

#include <string>

#import <Foundation/Foundation.h>

#include "Aether/ADT.h"

namespace xui {

inline void* retain(id obj) { return (__bridge_retained void*)obj; }

inline id release(void* obj) { return (__bridge_transfer id)obj; }

inline id transfer(void* obj) { return (__bridge id)obj; }

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

inline xui::Rect fromAppkitCoords(NSRect rect) {
    return { fromAppkitCoords(rect.origin),
             Size{ rect.size.width, rect.size.height } };
}

inline xui::Rect fromAppkitCoords(NSRect rect, double height) {
    return { fromAppkitCoords(rect.origin, height),
             Size{ rect.size.width, rect.size.height } };
}

inline xui::Rect fromNSRect(NSRect rect) {
    return { { rect.origin.x, rect.origin.y },
             { rect.size.width, rect.size.height } };
}

NSString* toNSString(std::string const& str);

NSString* toNSString(char const* str);

} // namespace xui

#endif // AETHER_MACOS_MACOSUTIL_H
