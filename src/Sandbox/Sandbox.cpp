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
              xui::Spacer(), xui::TextField("Input 1"),
              xui::TextField("Input 2"), xui::TextField("Input 3"),
              HScroller() });
    }

    std::unique_ptr<xui::View> HScroller() {
        return xui::HScrollView({
            xui::Button("Placeholder"),
            xui::Button("Placeholder"),
            xui::Button("Placeholder"),
            xui::Button("Placeholder"),
            xui::Button("Placeholder"),
        });
    }

    std::unique_ptr<xui::View> DetailPanel() {
        auto frameSetter = [&] {
            window->setFrame({ windowPos, { 500, 500 } }, true);
            windowPos.x += 100;
            windowPos.y += 100;
        };
        auto printText = [this] {
            std::cout << (textField ? textField->getText() : "") << std::endl;
        };
        return xui::VScrollView(
            { xui::Button("Print", printText) | xui::XFlex(),
              xui::Button("B", [] { std::cout << "B\n"; }) | xui::XFlex(),
              xui::Button("C", frameSetter) | xui::XFlex(),
              textField.assign(xui::TextField("Input")) });
    }

    std::unique_ptr<xui::Window> window;
    xui::Position windowPos{};
    xui::WeakRef<xui::TextFieldView> textField;
};

} // namespace

std::unique_ptr<xui::Application> xui::createApplication() {
    return std::make_unique<Sandbox>();
}
