#include <Aether/Application.h>
#include <Aether/View.h>
#include <Aether/Window.h>

using namespace xui;

namespace {

struct UITest: Application {
    UITest();
    
    std::unique_ptr<View> ControlsTest();
    std::unique_ptr<View> ButtonsTest();
    
    std::unique_ptr<Window> window;
};

}

UITest::UITest() {
    window = xui::window("UI Test", { { 100, 100 }, { 800, 600 } });
    window->setContentView(Tab({
        { "Controls", ControlsTest() }
    }));
}

std::unique_ptr<View> UITest::ControlsTest() {
    return HStack({
        ButtonsTest(),
        VSplit({
            std::make_unique<ColorView>(Color::Red()),
            std::make_unique<ColorView>(Color::Green()),
            std::make_unique<ColorView>(Color::Blue())
        })
    });
}

std::unique_ptr<View> UITest::ButtonsTest() {
    return VStack({
        Button("Hello")
    });
}

std::unique_ptr<Application> xui::createApplication() {
    return std::make_unique<UITest>();
}
