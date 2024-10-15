#include "Aether/Toolbar.h"

#include "Aether/View.h"

using namespace xui;

ToolbarView::~ToolbarView() = default;

std::unique_ptr<ToolbarView> xui::Toolbar(UniqueVector<View> views) {
    return std::make_unique<ToolbarView>(std::move(views));
}
