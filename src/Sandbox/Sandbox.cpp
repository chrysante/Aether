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

static std::vector<float2> nodeShape(Node const& node, Size size) {
    std::vector<float2> result;
    float const cornerRadius = 20;
    float const pinSize = 30;
    float const pinRadius = 10;
    auto vertexEmitter = [&](float2 v) { result.push_back(v); };
    static constexpr float pi = vml::constants<float>::pi;
    pathCircleSegment({ 0, cornerRadius }, { cornerRadius, cornerRadius },
                      pi / 2, vertexEmitter,
                      { .orientation = Orientation::Clockwise });
    pathCircleSegment({ size.width() - cornerRadius, 0 },
                      { size.width() - cornerRadius, cornerRadius }, pi / 2,
                      vertexEmitter, { .orientation = Orientation::Clockwise });
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
                      { .orientation = Orientation::Clockwise });
    vertexEmitter({ size.width() - cornerRadius, size.height() });
    pathCircleSegment({ cornerRadius, size.height() },
                      { cornerRadius, size.height() - cornerRadius }, pi / 2,
                      vertexEmitter, { .orientation = Orientation::Clockwise });
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
        addEventHandler([this](MouseDownEvent const& e) {
            if (e.mouseButton() == MouseButton::Left) {
                orderFront();
                return true;
            }
            return false;
        });
        addEventHandler([this](MouseDragEvent const& e) {
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
        ctx->addPolygon(shape,
                        { .fill = Gradient{ .begin{ { 0, 0 }, Color::Orange() },
                                            .end{ { 0, 2 * size().height() },
                                                  Color::Red() } } },
                        { .isYMonotone = true,
                          .orientation = Orientation::Counterclockwise });
        ctx->draw();
    }

    bool clipsToBounds() const override { return false; }

    WeakRef<View> stack;
    Vec2<double> pos = {};
    Node node;
};

class NodeLayer: public View {
public:
    NodeLayer():
        View({ .layoutModeX = LayoutMode::Flex,
               .layoutModeY = LayoutMode::Flex }) {
        addSubview(std::make_unique<NodeView>(Vec2<double>{ 0, 0 }));
        addSubview(std::make_unique<NodeView>(Vec2<double>{ 100, 300 }));
    }

    void updatePosition(Vec2<double> delta) {
        position += delta;
        doLayout(frame());
    }

private:
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

class SelectionLayer: public View {
public:
    SelectionLayer() { ignoreMouseEvents(); }

    void update(std::optional<xui::Rect> optRect) {
        auto* ctx = getDrawingContext();
        if (optRect) {
            auto r = normalize(*optRect);
            float2 min = (Vec2<double>)r.origin();
            float2 max = min + (float2)(Vec2<double>)r.size();
            float2 points[] = { min, { max.x, min.y }, max, { min.x, max.y } };
            ctx->addPolygon(points, { .fill = Color::Green() },
                            { .isYMonotone = true,
                              .orientation = Orientation::Counterclockwise });
        }
        ctx->draw();
    }

private:
    void doLayout(xui::Rect frame) override { setFrame(frame); }
};

class NodeEditorView: public View {
public:
    NodeEditorView():
        View({ .layoutModeX = LayoutMode::Flex,
               .layoutModeY = LayoutMode::Flex }),
        nodeLayer(addSubview(std::make_unique<NodeLayer>())),
        selectionLayer(addSubview(std::make_unique<SelectionLayer>())) {
        addEventHandler([this](ScrollEvent const& e) {
            nodeLayer->updatePosition(e.delta());
            return true;
        });
        addEventHandler([this](MouseDownEvent const& e) {
            if (e.mouseButton() != MouseButton::Left) return false;
            selectionRect = Rect{ e.locationInWindow() - origin(), {} };
            selectionLayer->update(selectionRect);
            return true;
        });
        addEventHandler([this](MouseUpEvent const& e) {
            if (e.mouseButton() != MouseButton::Left) return false;
            selectionRect = std::nullopt;
            selectionLayer->update(selectionRect);
            return true;
        });
        addEventHandler([this](MouseDragEvent const& e) {
            switch (e.mouseButton()) {
            case MouseButton::Left:
                if (!selectionRect) return false;
                selectionRect->size() += e.delta();
                selectionLayer->update(selectionRect);
                return true;
            case MouseButton::Right:
                nodeLayer->updatePosition(e.delta());
                return true;
            default:
                return false;
            }
        });
    }

private:
    void doLayout(Rect frame) override {
        setFrame(frame);
        xui::Rect bounds = { {}, frame.size() };
        nodeLayer->layout(bounds);
        selectionLayer->layout(bounds);
    }

    NodeLayer* nodeLayer;
    SelectionLayer* selectionLayer;
    std::optional<xui::Rect> selectionRect;
};

struct Sandbox: Application {
    Sandbox() { createWindow()->setContentView(NodeEditor()); }

    Window* createWindow() {
        auto w = xui::window("My Window", { { 100, 100 }, { 1000, 800 } },
                             { .fullSizeContentView = true });
        auto* ptr = w.get();
        windows.push_back(std::move(w));
        return ptr;
    }

    std::unique_ptr<View> Sidebar() {
        return VStack({ VStack({}) | PreferredHeight(38) | YStatic(),
                        VScrollView(
                            { Button("A") | XFlex(), Button("B") | XFlex() }) |
                            NoBackground }) |
               BlendBehindWindow;
    }

    std::unique_ptr<View> NodeEditor() {
        return HSplit({ Sidebar(), std::make_unique<NodeEditorView>() }) |
               SplitViewResizeStrategy::CutRight;
    }

    std::vector<std::unique_ptr<Window>> windows;
};

} // namespace

std::unique_ptr<Application> xui::createApplication() {
    return std::make_unique<Sandbox>();
}
