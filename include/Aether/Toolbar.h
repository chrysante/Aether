#ifndef AETHER_TOOLBAR_H
#define AETHER_TOOLBAR_H

#include <memory>
#include <span>
#include <vector>

#include <Aether/ADT.h>

namespace xui {

class View;

class ToolbarView {
public:
    explicit ToolbarView(std::vector<std::unique_ptr<View>> views);

    ~ToolbarView();

    void* nativeHandle() const { return _native; }

    void layout(Rect frame);

    struct Impl;

private:
    void* _native;
    std::vector<std::unique_ptr<View>> _views;
};

std::unique_ptr<ToolbarView> Toolbar(UniqueVector<View> views);

template <size_t N>
std::unique_ptr<ToolbarView> Toolbar(std::unique_ptr<View> (&&views)[N]) {
    return Toolbar(toMoveOnlyVector(std::move(views)));
}

} // namespace xui

#endif // AETHER_TOOLBAR_H
