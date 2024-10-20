#ifndef AETHER_MACOS_MACOSUTIL_H
#define AETHER_MACOS_MACOSUTIL_H

#include <optional>
#include <string_view>

#import <Appkit/NSColor.h>
#import <Foundation/Foundation.h>

#include "Aether/ADT.h"
#include "Aether/Vec.h"

namespace xui {

inline void* retain(id obj) { return (__bridge_retained void*)obj; }

inline id release(void* obj) { return (__bridge_transfer id)obj; }

inline id transfer(void* obj) { return (__bridge id)obj; }

inline NSSize toNSSize(xui::Size size) {
    return { size.width(), size.height() };
}

inline xui::Size fromNSSize(NSSize size) { return { size.width, size.height }; }

inline NSRect toNSRect(xui::Rect rect) {
    return NSMakeRect(rect.origin().x, rect.origin().y, rect.width(),
                      rect.height());
}

NSPoint toAppkitCoords(xui::Point pos);

inline NSPoint toAppkitCoords(xui::Point pos, double height) {
    return { pos.x, height - pos.y };
}

inline NSRect toAppkitCoords(xui::Rect rect) {
    auto pos = toAppkitCoords(rect.origin());
    pos.y -= rect.height();
    return { pos, NSSize{ rect.width(), rect.height() } };
}

inline NSRect toAppkitCoords(xui::Rect rect, double height) {
    auto pos = toAppkitCoords(rect.origin(), height);
    pos.y -= rect.height();
    return { pos, NSSize{ rect.width(), rect.height() } };
}

Point fromAppkitCoords(NSPoint pos);

inline Point fromAppkitCoords(NSPoint pos, double height) {
    return { pos.x, height - pos.y };
}

xui::Rect fromAppkitCoords(NSRect rect);

xui::Rect fromAppkitCoords(NSRect rect, double height);

inline xui::Rect fromNSRect(NSRect rect) {
    return { { rect.origin.x, rect.origin.y },
             { rect.size.width, rect.size.height } };
}

NSString* toNSString(std::string_view str);

std::string toStdString(NSString* str);

NSColor* toNSColor(Color const& color);

inline NSColor* toNSColor(std::optional<Color> const& color) {
    return color ? toNSColor(*color) : nil;
}

Color fromNSColor(NSColor* nsColor);

void setAssocPointer(id object, void const* key, void* pointer);

void* getAssocPointer(id object, void const* key);

template <typename T>
    requires std::is_pointer_v<T>
T getAssocPointer(id object, void const* key) {
    return static_cast<T>(getAssocPointer(object, key));
}

} // namespace xui

#endif // AETHER_MACOS_MACOSUTIL_H
