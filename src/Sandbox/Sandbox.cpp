#include <iostream>
#include <span>
#include <vector>

#include <utl/vector.hpp>

#include <Aether/Application.h>
#include <Aether/DrawingContext.h>
#include <Aether/Modifiers.h>
#include <Aether/Shapes.h>
#include <Aether/Toolbar.h>
#include <Aether/View.h>
#include <Aether/Window.h>

using namespace xui;
using namespace vml::short_types;

namespace {

class Pin {};

class InputPin: public Pin {};

class OutputPin: public Pin {};

class Node {
public:
    Node(): _inputs(2), _outputs(2) {}

    std::span<InputPin const> inputs() const { return _inputs; }
    std::span<OutputPin const> outputs() const { return _outputs; }

private:
    utl::small_vector<InputPin> _inputs;
    utl::small_vector<OutputPin> _outputs;
};

static Size computeMinSize(Node const& node) { return { 200, 100 }; }

static std::vector<xui::Point> nodeShape(Node const& node, Size size) {
    std::vector<xui::Point> result;
    float const cornerRadius = 20;
    float const pinSize = 30;
    float const pinRadius = 10;
    auto vertexEmitter = [&](float2 v) { result.push_back(v); };
    static constexpr float pi = vml::constants<float>::pi;
    pathCircleSegment({ 0, cornerRadius }, { cornerRadius, cornerRadius },
                      pi / 2, vertexEmitter,
                      { .orientation = CircleSegmentOptions::CW });
    pathCircleSegment({ size.width() - cornerRadius, 0 },
                      { size.width() - cornerRadius, cornerRadius }, pi / 2,
                      vertexEmitter,
                      { .orientation = CircleSegmentOptions::CW });
    double cursor = cornerRadius;
    for ([[maybe_unused]] auto& pin: node.outputs()) {
        pathCircleSegment({ size.width(), cursor + pinSize / 2 - pinRadius },
                          { size.width(), cursor + pinSize / 2 }, pi,
                          vertexEmitter);
        cursor += pinSize;
    }
    pathCircleSegment(float2{ size.width(), size.height() - cornerRadius },
                      float2(size.width(), size.height()) -
                          float2(cornerRadius),
                      pi / 2, vertexEmitter,
                      { .orientation = CircleSegmentOptions::CW });
    pathCircleSegment({ cornerRadius, size.height() },
                      { cornerRadius, size.height() - cornerRadius }, pi / 2,
                      vertexEmitter,
                      { .orientation = CircleSegmentOptions::CW });
    cursor = cornerRadius + pinSize * node.inputs().size();
    for ([[maybe_unused]] auto& pin: node.inputs()) {
        pathCircleSegment({ 0, cursor - pinSize / 2 + pinRadius },
                          { 0, cursor - pinSize / 2 }, pi, vertexEmitter);
        cursor -= pinSize;
    }
    return result;
}

class NodeView: public View {
public:
    explicit NodeView(Vec2<double> pos): pos(pos) {
        stack = addSubview(VStack({}));
        setPreferredSize({ 300, 100 });
        setShadow();
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
        draw({});
    }

    void draw(xui::Rect) override {
        auto* ctx = getDrawingContext();
        auto shape = nodeShape(node, preferredSize());
        ctx->addPolygon(shape);
        ctx->draw();
    }

    bool clipsToBounds() const override { return false; }

    WeakRef<View> stack;
    Vec2<double> pos = {};
    Node node;
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
        window = xui::window("My Window", { { 100, 100 }, { 1000, 800 } },
                             { .fullSizeContentView = true });
    }

    std::unique_ptr<View> Sidebar() {
        return VStack({ VStack({}) | PreferredHeight(38) | YStatic(),
                        VScrollView(
                            { Button("A") | XFlex(), Button("B") | XFlex() }) |
                            NoBackground }) |
               BlendBehindWindow;
    }

    std::unique_ptr<View> makeNodeEditor() {
        return HSplit({ Sidebar(), std::make_unique<NodeEditorView>() }) |
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
