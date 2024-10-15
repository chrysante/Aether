#include "Aether/Toolbar.h"

#include <charconv>

#include <AppKit/AppKit.h>

#include "Aether/MacOS/MacOSUtil.h"
#include "Aether/View.h"

using namespace xui;

namespace xui {

extern void* ToolbarHandleKey;

}

void* xui::ToolbarHandleKey = &ToolbarHandleKey;

struct ToolbarView::Impl {
    static NSToolbarItem* makeItem(ToolbarView& toolbar, size_t index,
                                   NSToolbarItemIdentifier itemIdentifier) {
        if (index >= toolbar._views.size()) {
            return nil;
        }
        NSToolbarItem* item =
            [[NSToolbarItem alloc] initWithItemIdentifier:itemIdentifier];

        auto* view = toolbar._views[index].get();
        item.view = transfer(view->nativeHandle());
        return item;
    }

    static size_t numItems(ToolbarView const& toolbar) {
        return toolbar._views.size();
    }
};

@interface AetherToolbarDelegate: NSObject <NSToolbarDelegate>
@end

@implementation AetherToolbarDelegate
- (NSToolbarItem*)toolbar:(NSToolbar*)native
        itemForItemIdentifier:(NSToolbarItemIdentifier)itemIdentifier
    willBeInsertedIntoToolbar:(BOOL)flag {
    auto ID = toStdString(itemIdentifier);
    size_t index{};
    auto result = std::from_chars(ID.data(), ID.data() + ID.size(), index);
    if (result.ec != std::error_code()) {
        return nil;
    }
    auto* toolbar = getAssocPointer<ToolbarView*>(native, ToolbarHandleKey);
    return ToolbarView::Impl::makeItem(*toolbar, index, itemIdentifier);
}

- (NSArray<NSString*>*)toolbarAllowedItemIdentifiers:(NSToolbar*)toolbar {
    return [self toolbarDefaultItemIdentifiers:toolbar];
}

- (NSArray<NSString*>*)toolbarDefaultItemIdentifiers:(NSToolbar*)native {
    auto* toolbar = getAssocPointer<ToolbarView*>(native, ToolbarHandleKey);
    NSMutableArray* array = [[NSMutableArray alloc] init];
    size_t numItems = ToolbarView::Impl::numItems(*toolbar);
    for (size_t i = 0; i < numItems; ++i) {
        [array addObject:toNSString(std::to_string(i))];
    }
    return array;
}

@end

AetherToolbarDelegate* gDelegate = [[AetherToolbarDelegate alloc] init];

ToolbarView::ToolbarView(std::vector<std::unique_ptr<View>> views):
    _views(std::move(views)) {
    NSToolbar* toolbar = [[NSToolbar alloc] init];
    setAssocPointer(toolbar, ToolbarHandleKey, this);
    toolbar.delegate = gDelegate;
    _native = retain(toolbar);
}

void ToolbarView::layout(Rect frame) { _views[0]->layout(frame); }
