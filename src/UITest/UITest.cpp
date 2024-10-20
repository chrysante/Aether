#include <iostream>

#include <Aether/Application.h>
#include <Aether/Modifiers.h>
#include <Aether/View.h>
#include <Aether/Window.h>

using namespace xui;

namespace {

struct UITest: Application {
    UITest();

    std::unique_ptr<View> ButtonsTest();
    std::unique_ptr<View> TestView();
    std::unique_ptr<View> Sidebar();
    std::unique_ptr<View> DetailPanel();

    std::unique_ptr<Window> window;
    Position windowPos{};
    WeakRef<TextFieldView> textField;
    WeakRef<SplitView> splitView;
};

} // namespace

UITest::UITest() {
    window = xui::window("UI Test", { { 100, 100 }, { 800, 600 } });
    window->setContentView(
        Tab({ { "Buttons", ButtonsTest() }, { "Test View", TestView() } }));
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

std::unique_ptr<View> UITest::TestView() {
    auto scrollHandler = [](ScrollEvent const& event) {
        std::cout << "Location: " << event.locationInWindow() << "\n";
        std::cout << "Delta:    " << event.delta() << "\n";
        return false;
    };
    auto dragHandler = [](MouseDragEvent const& event) {
        std::cout << "Location: " << event.locationInWindow() << "\n";
        std::cout << "Delta:    " << event.delta() << "\n";
        return false;
    };
    auto mouseClickHandler = [](MouseClickEvent const& event) {
        std::cout << "Location: " << event.locationInWindow() << "\n";
        return false;
    };
    auto moveHandler = [](MouseMoveEvent const&) {
        std::cout << "." << std::flush;
        return false;
    };
    auto transitionHandler = [](MouseTransitionEvent const& event) {
        // clang-format off
        csp::visit(event, csp::overload{
            [](MouseEnterEvent const&) { std::cout << "Enter\n"; },
            [](MouseExitEvent const&) { std::cout << "Exit\n"; }
        }); // clang-format on
        return false;
    };
    return HSplit({
               VSplit({
                   std::make_unique<ColorView>(Color::Red()) |
                       OnEvent(mouseClickHandler) | OnEvent(dragHandler),
                   std::make_unique<ColorView>(Color::Green()) |
                       MinHeight(100) | SplitViewCollapsable |
                       OnEvent(scrollHandler),
                   std::make_unique<ColorView>(Color::Blue()) |
                       OnEvent(transitionHandler, moveHandler) |
                       TrackMouseMovement(MouseTrackingKind::Movement |
                                              MouseTrackingKind::Transition,
                                          MouseTrackingActivity::ActiveWindow),
               }) | SplitterStyle::Thick |
                   MinWidth(120) | SplitViewCollapsable,
               Sidebar() | OnEvent(scrollHandler) | SplitViewCollapsable(false),
               DetailPanel() | MinWidth(150) | SplitViewCollapsable,
               std::make_unique<ColorView>(Color::Red()) | MinWidth(100),
           }) |
           SplitViewResizeStrategy::Proportional | AssignTo(splitView);
}

std::unique_ptr<View> UITest::Sidebar() {
    auto hScroller = HScrollView({
        Button("Placeholder"),
        Button("Placeholder") | AlignY::Center,
        Button("Placeholder") | AlignY::Bottom,
        Button("Placeholder") | AlignY::Center,
        Button("Placeholder"),
    });
    return VStack(
               { Button("Option 1", [] { std::cout << "Hello\n"; }) |
                     PreferredWidth(100) | AlignX::Center,
                 Button("Option 2", [] { std::cout << "Hello\n"; }) | XFlex(),
                 Button("Option 3", [] { std::cout << "Hello\n"; }) |
                     AlignX::Right,
                 Spacer(), TextField("Input 1"), TextField("Input 2"),
                 TextField("Input 3"), std::move(hScroller) }) |
           MinWidth(200);
}

static std::unique_ptr<View> LabelledSwitch(std::string label) {
    return HStack({ Label(std::move(label)) | AlignY::Center, Spacer(),
                    std::make_unique<SwitchView>() | AlignY::Center }) |
           XFlex() | PreferredHeight(34) | PaddingX(6) | PaddingY(6);
}

std::unique_ptr<View> UITest::DetailPanel() {
    auto frameSetter = [&] {
        window->setFrame({ windowPos, { 500, 500 } }, true);
        windowPos.x += 100;
        windowPos.y += 100;
    };
    auto printText = [this] {
        std::cout << (textField ? textField->getText() : "") << std::endl;
    };
    auto cycleSplitStyle = [this] {
        if (!splitView) {
            return;
        }
        auto curr = splitView->splitterStyle();
        auto next = curr == SplitterStyle::Pane ? SplitterStyle::Thin :
                                                  SplitterStyle((int)curr + 1);
        splitView->setSplitterStyle(next);
    };
    return VScrollView(
        { Button("Print", printText) | XFlex(),
          Button("Cycle Splitstyle", cycleSplitStyle) | XFlex(),
          Button("C", frameSetter) | XFlex(), ProgressBar() | PaddingX(8),
          ProgressSpinner() | MinSize({ 38, 38 }) | PaddingX(8),
          TextField("Input") | AssignTo(textField),
          std::make_unique<SwitchView>(), LabelledSwitch("Some Switch") });
}

std::unique_ptr<Application> xui::createApplication() {
    return std::make_unique<UITest>();
}
