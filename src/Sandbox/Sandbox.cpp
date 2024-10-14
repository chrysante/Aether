#include <iostream>

#include <Aether/Application.h>
#include <Aether/Modifiers.h>
#include <Aether/View.h>
#include <Aether/Window.h>

using namespace xui;

namespace {

struct Sandbox: Application {
    Sandbox() {
        window = xui::window("My Window", { { 100, 100 }, { 500, 500 } });
        // auto content = HStack({
        //     DetailPanel(),
        //     Sidebar(),
        // });

        window->setContentView(
            HSplit({
                VSplit({
                    std::make_unique<ColorView>(Color::Red()),
                    std::make_unique<ColorView>(Color::Green()) |
                        MinHeight(100) | SplitViewCollapsable,
                    std::make_unique<ColorView>(Color::Blue()),
                }) | SplitterStyle::Thick |
                    MinWidth(120) | SplitViewCollapsable,
                Sidebar() | SplitViewCollapsable(false),
                DetailPanel() | MinWidth(150) | SplitViewCollapsable,
                std::make_unique<ColorView>(Color::Red()) | MinWidth(100),
            }) |
            SplitViewResizeStrategy::Proportional | AssignTo(splitView));
    }

    std::unique_ptr<View> Sidebar() {
        return VStack({ Button("Option 1", [] { std::cout << "Hello\n"; }) |
                            PreferredWidth(100),
                        Button("Option 2", [] { std::cout << "Hello\n"; }) |
                            XFlex(),
                        Button("Option 3", [] { std::cout << "Hello\n"; }),
                        Spacer(), TextField("Input 1"), TextField("Input 2"),
                        TextField("Input 3"), HScroller() }) |
               MinWidth(200);
    }

    std::unique_ptr<View> HScroller() {
        return HScrollView({
            Button("Placeholder"),
            Button("Placeholder"),
            Button("Placeholder"),
            Button("Placeholder"),
            Button("Placeholder"),
        });
    }

    std::unique_ptr<View> DetailPanel() {
        auto frameSetter = [&] {
            window->setFrame({ windowPos, { 500, 500 } }, true);
            windowPos.x += 100;
            windowPos.y += 100;
        };
        auto printText = [this] {
            std::cout << (textField ? textField->getText() : "") << std::endl;
        };
        auto cycleSplitStyle = [this] {
            auto curr = splitView->splitterStyle();
            auto next = curr == SplitterStyle::Pane ?
                            SplitterStyle::Thin :
                            SplitterStyle((int)curr + 1);
            splitView->setSplitterStyle(next);
        };
        return VScrollView(
            { Button("Print", printText) | XFlex(),
              Button("Cycle Splitstyle", cycleSplitStyle) | XFlex(),
              Button("C", frameSetter) | XFlex(),
              ProgressBar() | XFlex() | PaddingX(8),
              ProgressSpinner() | XFlex() | PaddingX(8),
              textField.assign(TextField("Input")) });
    }

    std::unique_ptr<Window> window;
    Position windowPos{};
    WeakRef<TextFieldView> textField;
    WeakRef<SplitView> splitView;
};

} // namespace

std::unique_ptr<Application> xui::createApplication() {
    return std::make_unique<Sandbox>();
}
