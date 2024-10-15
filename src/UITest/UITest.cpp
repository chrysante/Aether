#include <Aether/Application.h>
#include <Aether/Modifiers.h>
#include <Aether/View.h>
#include <Aether/Window.h>

using namespace xui;

namespace {

struct UITest: Application {
    UITest();

    std::unique_ptr<View> ButtonsTest();

    std::unique_ptr<Window> window;
};

} // namespace

UITest::UITest() {
    window = xui::window("UI Test", { { 100, 100 }, { 800, 600 } });
    window->setContentView(Tab({ { "Buttons", ButtonsTest() } }));
}

std::unique_ptr<View> UITest::ButtonsTest() {
    auto label = Label("");
    auto buttonVerifier = [&, &label = *label](std::string text) {
        return [text, view = WeakRef(label)] {
            if (view) {
                view->setText(text);
            }
        };
    };
    auto ButtonList = [&](BezelStyle style, std::string name) {
        return VStack({
            Label(name),
            Button("Button", buttonVerifier("Button")) | style,
            ToggleButton("ToggleButton", buttonVerifier("ToggleButton")) |
                style,
            SwitchButton("SwitchButton", buttonVerifier("SwitchButton")) |
                style,
            RadioButton("RadioButton", buttonVerifier("RadioButton")) | style,
        });
    };
    return HStack({
        VStack({ Button("Hello", buttonVerifier("Hello")),
                 RadioButton("Hello 1", buttonVerifier("Hello 1")),
                 RadioButton("Hello 2", buttonVerifier("Hello 2")),
                 RadioButton("Hello 3", buttonVerifier("Hello 3")), Spacer(),
                 std::move(label) }),
        ButtonList(BezelStyle::Push, "Push"),
        ButtonList(BezelStyle::PushFlexHeight, "PushFlexHeight"),
        ButtonList(BezelStyle::Circular, "Circular"),
        ButtonList(BezelStyle::Help, "Help"),
        ButtonList(BezelStyle::SmallSquare, "SmallSquare"),
        ButtonList(BezelStyle::Toolbar, "Toolbar"),
        ButtonList(BezelStyle::Badge, "Badge"),
    });
}

std::unique_ptr<Application> xui::createApplication() {
    return std::make_unique<UITest>();
}
