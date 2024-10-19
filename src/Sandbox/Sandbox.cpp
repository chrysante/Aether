#include <iostream>

#include <Aether/Application.h>
#include <Aether/Modifiers.h>
#include <Aether/Toolbar.h>
#include <Aether/View.h>
#include <Aether/Window.h>

using namespace xui;

namespace {

class NodeView: public View {
public:
    explicit NodeView(Vec2<double> pos): pos(pos) {
        stack = addSubview(VStack({
            Button("One", [] { std::cout << "One\n"; }),
            Button("Two", [] { std::cout << "Two\n"; }),
        }));
        setPreferredSize({ 200, 100 });
        onEvent([this](MouseDownEvent const& e) {
            if (e.mouseButton() == MouseButton::Left) {
                orderFront();
                return true;
            }
            return false;
        });
        onEvent([this](MouseDragEvent const& e) {
            if (e.mouseButton() != MouseButton::Left) return false;
            this->pos += e.delta();
            parent()->layout(parent()->frame());
            return true;
        });
    }

    Vec2<double> position() const { return pos; }

private:
    void doLayout(xui::Rect frame) override {
        setFrame(frame);
        stack->layout({ {}, frame.size() });
    }

    WeakRef<View> stack;
    Vec2<double> pos = {};
};

class NodeEditorView: public View {
public:
    NodeEditorView():
        View({ .layoutModeX = LayoutMode::Flex,
               .layoutModeY = LayoutMode::Flex }) {
        addSubview(std::make_unique<NodeView>(Vec2<double>{ 0, 0 }));
        addSubview(std::make_unique<NodeView>(Vec2<double>{ 100, 300 }));
        onEvent([this](ScrollEvent const& e) {
            updatePosition(e.delta());
            return true;
        });
        onEvent([this](MouseDragEvent const& e) {
            if (e.mouseButton() != MouseButton::Right) return false;
            updatePosition(e.delta());
            return true;
        });
    }

private:
    void updatePosition(Vec2<double> delta) {
        position += delta;
        doLayout(frame());
    }

    void doLayout(Rect frame) override {
        setFrame(frame);
        for (auto* view: subviews()) {
            if (auto* node = dynamic_cast<NodeView*>(view)) {
                node->layout(
                    { position + node->position(), node->preferredSize() });
            }
        }
    }

    Vec2<double> position = {};
};

struct Sandbox: Application {
    Sandbox() {
        createWindow();
        window->setContentView(makeNodeEditor());
    }

    void createWindow() {
        window = xui::window("My Window", { { 100, 100 }, { 500, 500 } },
                             { .fullSizeContentView = true });
    }

    std::unique_ptr<View> Sidebar() {
        return VStack({ VStack({}) | PreferredHeight(38) | YStatic(),
                        VScrollView({ Button("A"), Button("B") }) |
                            NoBackground }) |
               BlendBehindWindow;
    }

    std::unique_ptr<View> makeNodeEditor() {
        return HSplit({ Sidebar(), std::make_unique<NodeEditorView>(),
                        Sidebar() }) |
               SplitViewResizeStrategy::CutRight;
    }

    std::unique_ptr<View> OtherTestView() {
        return VSplit({
            HStack({}),
            HStack({}) | OnEvent([](MouseDownEvent const&) {
            std::cout << "Down\n";
            return false;
        }) | OnEvent([](MouseUpEvent const&) {
            std::cout << "Up\n";
            return false;
        }),
        });
    }

    std::unique_ptr<Window> window;
};

} // namespace

std::unique_ptr<Application> xui::createApplication() {
    return std::make_unique<Sandbox>();
}
