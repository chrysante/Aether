#include <iostream>

#include <Aether/Application.h>
#include <Aether/View.h>
#include <Aether/Window.h>

namespace {

struct Sandbox: xui::Application {
    Sandbox() {
        window = xui::window("My Window", { { 100, 100 }, { 500, 500 } });
        auto content = xui::hStack({
            detailPanel(),
            sidebar(),
        });
        window->setContentView(std::move(content));
    }

    std::unique_ptr<xui::View> sidebar() {
        return xui::vStack(
            { xui::button("Option 1", [] { std::cout << "Hello\n"; }),
              xui::button("Option 2", [] { std::cout << "Hello\n"; }),
              xui::button("Option 3", [] { std::cout << "Hello\n"; }),
              xui::button("Option 4", [] { std::cout << "Hello\n"; }) });
    }

    std::unique_ptr<xui::View> detailPanel() {
        return xui::scrollView({
            xui::button("A", [] { std::cout << "A\n"; }),
            xui::button("B", [] { std::cout << "B\n"; }),
            xui::button("C",
                        [&] {
            window->setFrame({ windowPos, { 500, 500 } }, true);
            windowPos.x += 100;
            windowPos.y += 100;
        }),
        });
    }

    std::unique_ptr<xui::Window> window;
    xui::Position windowPos{};
};

} // namespace

std::unique_ptr<xui::Application> xui::createApplication() {
    return std::make_unique<Sandbox>();
}
