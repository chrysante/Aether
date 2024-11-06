#include "Flow/Editor.h"

#include <optional>

#include <Aether/DrawingContext.h>
#include <Aether/Shapes.h>
#include <utl/hashtable.hpp>

#include "Flow/Graph.h"

using namespace flow;
using namespace xui;
using namespace vml::short_types;

float const CornerRadius = 10;
float const PinSize = 15;
float const PinRadius = 5;

static xui::Size computeNodeSize(Node const&) { return { 200, 100 }; }

static std::vector<float2> nodeShape(Node const& node, Size size) {
    std::vector<float2> result;

    auto vertexEmitter = [&](float2 v) { result.push_back(v); };
    static constexpr float pi = vml::constants<float>::pi;
    pathCircleSegment({ 0, CornerRadius }, { CornerRadius, CornerRadius },
                      pi / 2, vertexEmitter,
                      { .orientation = Orientation::Clockwise });
    pathCircleSegment({ size.width() - CornerRadius, 0 },
                      { size.width() - CornerRadius, CornerRadius }, pi / 2,
                      vertexEmitter, { .orientation = Orientation::Clockwise });
    double cursor = CornerRadius;
    for ([[maybe_unused]] auto* pin: node.outputs()) {
        pathCircleSegment({ size.width(), cursor + PinSize / 2 - PinRadius },
                          { size.width(), cursor + PinSize / 2 }, pi,
                          vertexEmitter);
        cursor += PinSize;
    }
    pathCircleSegment(float2{ size.width(), size.height() - CornerRadius },
                      float2(size.width(), size.height()) -
                          float2(CornerRadius),
                      pi / 2, vertexEmitter,
                      { .orientation = Orientation::Clockwise });
    vertexEmitter({ size.width() - CornerRadius, size.height() });
    pathCircleSegment({ CornerRadius, size.height() },
                      { CornerRadius, size.height() - CornerRadius }, pi / 2,
                      vertexEmitter, { .orientation = Orientation::Clockwise });
    cursor = CornerRadius + PinSize * node.inputs().size();
    for ([[maybe_unused]] auto* pin: node.inputs()) {
        pathCircleSegment({ 0, cursor - PinSize / 2 + PinRadius },
                          { 0, cursor - PinSize / 2 }, pi, vertexEmitter);
        cursor -= PinSize;
    }
    return result;
}

namespace {

class NodeView: public xui::View {
public:
    explicit NodeView(Node& node): _node(node) {
        addSubview(VStack({})); // FIXME: Shadows don't work without this
        setShadow();
        label = addSubview(Label(StringProxy::Reference(node.name())));
    }

    Node& node() const { return _node; }

    xui::Point position() const { return node().position(); }

    xui::Size size() const { return computeNodeSize(node()); }

private:
    void doLayout(xui::Rect frame) override;

    void draw(xui::Rect) override;

    bool onEvent(MouseDownEvent const& e) override {
        if (e.mouseButton() == MouseButton::Left) {
            orderFront();
            return true;
        }
        return false;
    }

    bool onEvent(MouseDragEvent const& e) override {
        if (e.mouseButton() != MouseButton::Left) return false;
        node().setPosition(node().position() + e.delta());
        parent()->layout(parent()->frame());
        return true;
    }

    bool clipsToBounds() const override { return false; }

    Node& _node;
    LabelView* label = nullptr;
};

} // namespace

void NodeView::doLayout(xui::Rect frame) {
    setFrame(frame);
    draw({});
    label->layout({ { 0, frame.size().y }, { 100, 20 } });
}

void NodeView::draw(xui::Rect) {
    auto* ctx = getDrawingContext();
    auto shape = nodeShape(node(), size());
    ctx->addPolygon(shape,
                    { .fill = Gradient{ .begin{ { 0, 0 }, Color::Orange() },
                                        .end{ { 0, 2 * size().height() },
                                              Color::Red() } } },
                    { .isYMonotone = true,
                      .orientation = Orientation::Counterclockwise });
    ctx->draw();
}

class flow::NodeLayerView: public xui::View {
public:
    explicit NodeLayerView(EditorView& editor): editor(editor) {}

    void setGraph(Graph* g);

    NodeView* getNodeView(Node const* node) const {
        auto itr = viewMap.find(node);
        assert(itr != viewMap.end());
        return itr->second;
    }

private:
    void doLayout(xui::Rect frame) override;

    void draw(xui::Rect) override;

    void drawLines(DrawingContext* ctx);

    Point getPinLocation(Pin const& pin) const;

    EditorView& editor;
    Graph* graph = nullptr;

    utl::hashmap<Node const*, NodeView*> viewMap;
};

void NodeLayerView::setGraph(Graph* g) {
    graph = g;
    removeAllSubviews();
    viewMap.clear();
    if (!graph) return;
    for (auto* node: graph->nodes()) {
        auto* view = addSubview(std::make_unique<NodeView>(*node));
        viewMap.insert({ node, view });
    }
}

void NodeLayerView::doLayout(xui::Rect frame) {
    setFrame(frame);
    if (!graph) return;
    draw({});
    for (auto* node: graph->nodes()) {
        auto* nodeView = getNodeView(node);
        nodeView->layout({ editor.surfaceOrigin() + nodeView->position(),
                           nodeView->size() });
    }
}

void NodeLayerView::draw(xui::Rect) {
    auto* ctx = getDrawingContext();
    if (graph) {
        drawLines(ctx);
    }
    ctx->draw();
}

static void drawLine(DrawingContext* ctx, vml::float2 begin, vml::float2 end) {
    static constexpr size_t NumSegments = 20;
    std::array<float2, NumSegments + 1> vertices;
    float yDiff = std::abs(begin.y - end.y);
    float curve = 200 * 2 * std::atan(yDiff / 200) / vml::constants<float>::pi;
    pathBezier({ { begin, begin + float2(curve, 0), end - float2(curve, 0),
                   end } },
               [&, i = size_t{ 0 }](float2 p) mutable { vertices[i++] = p; },
               { .numSegments = NumSegments });
    ctx->addLine(vertices, { .fill = FlatColor(Color::Black()) },
                 { .width = 3,
                   .beginCap = { LineCapOptions::Circle },
                   .endCap = { LineCapOptions::Circle } });
}

void NodeLayerView::drawLines(DrawingContext* ctx) {
    assert(graph);
    for (auto* node: graph->nodes()) {
        for (auto* input: node->inputs()) {
            auto* source = input->source();
            if (!source) continue;
            float2 begin = (Vec2<double>)getPinLocation(*source);
            float2 end = (Vec2<double>)getPinLocation(*input);
            drawLine(ctx, begin, end);
        }
    }
}

Point NodeLayerView::getPinLocation(Pin const& pin) const {
    auto* node = pin.node();
    auto* nodeView = getNodeView(node);
    Point nodePos = node->position() + editor.surfaceOrigin();
    // clang-format off
    return visit(pin, csp::overload{
        [&](InputPin const& pin) {
            size_t index = node->getIndex(&pin);
            return nodePos + Vec2<double>(0, CornerRadius + PinSize * (index + 0.5));
        },
        [&](OutputPin const& pin) {
            size_t index = node->getIndex(&pin);
            return nodePos + Vec2<double>(nodeView->size().x, CornerRadius + PinSize * (index + 0.5));
        },
    }); // clang-format on
}

class flow::SelectionLayerView: public View {
public:
    SelectionLayerView() { ignoreMouseEvents(); }

    void setBegin(xui::Point pos) {
        rect = xui::Rect{ pos, {} };
        draw({});
    }
    void setEnd(xui::Point pos) {
        if (!rect) return;
        rect->size() = pos - rect->origin();
        draw({});
    }
    void clearRect() {
        rect = std::nullopt;
        draw({});
    }

private:
    void doLayout(xui::Rect frame) override { setFrame(frame); }

    void draw(xui::Rect) override;

    std::optional<xui::Rect> rect;
};

void SelectionLayerView::draw(xui::Rect) {
    auto* ctx = getDrawingContext();
    if (rect) {
        auto r = normalize(*rect);
        float2 min = (Vec2<double>)r.origin();
        float2 max = min + (float2)(Vec2<double>)r.size();
        float2 points[] = { min, { max.x, min.y }, max, { min.x, max.y } };
        ctx->addPolygon(points, { .fill = Color::Green() },
                        { .isYMonotone = true,
                          .orientation = Orientation::Counterclockwise });
    }
    ctx->draw();
}

EditorView::EditorView(Graph* graph):
    nodeLayer(addSubview(std::make_unique<NodeLayerView>(*this))),
    selectionLayer(addSubview(std::make_unique<SelectionLayerView>())) {
    setGraph(graph);
}

void EditorView::setGraph(Graph* graph) {
    _graph = graph;
    nodeLayer->setGraph(graph);
}

void EditorView::doLayout(xui::Rect frame) {
    setFrame(frame);
    nodeLayer->layout(bounds());
}

bool EditorView::onEvent(ScrollEvent const& e) {
    addOriginDelta(e.delta());
    return true;
}

bool EditorView::onEvent(MouseDownEvent const& e) {
    if (e.mouseButton() != MouseButton::Left) return false;
    selectionLayer->setBegin(e.locationInWindow() - origin());
    return true;
}

bool EditorView::onEvent(MouseUpEvent const& e) {
    if (e.mouseButton() != MouseButton::Left) return false;
    selectionLayer->clearRect();
    return true;
}

bool EditorView::onEvent(MouseDragEvent const& e) {
    switch (e.mouseButton()) {
    case MouseButton::Left:
        selectionLayer->setEnd(e.locationInWindow() - origin());
        return true;
    case MouseButton::Right:
        addOriginDelta(e.delta());
        return true;
    default:
        return false;
    }
}

void EditorView::addOriginDelta(Vec2<double> delta) {
    _origin += delta;
    layout(frame());
}
