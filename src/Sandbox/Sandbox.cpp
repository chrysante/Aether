#include <iostream>

#include <Aether/Application.h>
#include <Aether/Modifiers.h>
#include <Aether/View.h>
#include <Aether/Window.h>

namespace {

struct Sandbox: xui::Application {
    Sandbox() {
        window = xui::window("My Window", { { 100, 100 }, { 500, 500 } });
        auto content = xui::HStack({
            DetailPanel(),
            Sidebar(),
        });
        window->setContentView(std::move(content));
    }

    std::unique_ptr<xui::View> Sidebar() {
        return xui::VStack(
            { xui::Button("Option 1", [] { std::cout << "Hello\n"; }) |
                  xui::PreferredWidth(100),
              xui::Button("Option 2", [] { std::cout << "Hello\n"; }) |
                  xui::XFlex(),
              xui::Button("Option 3", [] { std::cout << "Hello\n"; }),
              xui::Spacer(), xui::TextField("Input"), HScroller() });
    }

    std::unique_ptr<xui::View> HScroller() {
        return xui::HScrollView({
            xui::Button("placeholder"),
            xui::Button("placeholder"),
            xui::Button("placeholder"),
            xui::Button("placeholder"),
            xui::Button("placeholder"),
        });
    }

    std::unique_ptr<xui::View> DetailPanel() {
        auto frameSetter = [&] {
            window->setFrame({ windowPos, { 500, 500 } }, true);
            windowPos.x += 100;
            windowPos.y += 100;
        };
        return xui::VScrollView(
            { xui::Button("Print", [] { std::cout << "A\n"; }) | xui::XFlex(),
              xui::Button("B", [] { std::cout << "B\n"; }) | xui::XFlex(),
              xui::Button("C", frameSetter) | xui::XFlex(),
              xui::TextField("Input") });
    }

    std::unique_ptr<xui::Window> window;
    xui::Position windowPos{};
};

} // namespace

std::unique_ptr<xui::Application> xui::createApplication() {
    return std::make_unique<Sandbox>();
}
