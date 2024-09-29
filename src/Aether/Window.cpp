#include "Aether/Window.h"

#include "Aether/View.h"

using namespace xui;

#if 0

Window::Window(std::string title, Rect rect):
_title(std::move(_title)), _rect(rect) {
    
}

void Window::setRect(Rect rect) {
    
}

void Window::setTitle(std::string  title) {
    
}

#endif

std::unique_ptr<Window> xui::window(std::string title, Rect frame,
                                    WindowProperties props,
                                    std::unique_ptr<View> content) {
    return std::make_unique<Window>(std::move(title), frame, props,
                                    std::move(content));
}
