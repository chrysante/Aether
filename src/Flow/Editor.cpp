#include "Flow/Editor.h"

#include <optional>

#include <Aether/DrawingContext.h>
#include <Aether/Shapes.h>
#include <utl/hashtable.hpp>

#include "Flow/Graph.h"

using namespace flow;
using namespace xui;
using namespace vml::short_types;

static xui::Size computeNodeSize(Node const&) { return { 200, 100 }; }

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
    for ([[maybe_unused]] auto* pin: node.outputs()) {
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
    for ([[maybe_unused]] auto* pin: node.inputs()) {
        pathCircleSegment({ 0, cursor - pinSize / 2 + pinRadius },
                          { 0, cursor - pinSize / 2 }, pi, vertexEmitter);
        cursor -= pinSize;
    }
    return result;
}

namespace {

class NodeView: public xui::View {
public:
    explicit NodeView(Node& node): _node(node) {
        addSubview(VStack({})); // FIXME: Shadows don't work without this
        setShadow();
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
};

} // namespace

void NodeView::doLayout(xui::Rect frame) {
    setFrame(frame);
    draw({});
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
    explicit NodeLayerView(EditorView* editor): editor(editor) {}

    void setGraph(Graph* g);

    NodeView* getNodeView(Node const* node) const {
        auto itr = viewMap.find(node);
        assert(itr != viewMap.end());
        return itr->second;
    }

private:
    void doLayout(xui::Rect frame) override;

    EditorView* editor = nullptr;
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
    for (auto* node: graph->nodes()) {
        auto* nodeView = getNodeView(node);
        nodeView->layout({ origin() + nodeView->position(), nodeView->size() });
    }
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
    nodeLayer(addSubview(std::make_unique<NodeLayerView>(this))),
    selectionLayer(addSubview(std::make_unique<SelectionLayerView>())) {
    setGraph(graph);
}

void EditorView::setGraph(Graph* graph) {
    _graph = graph;
    nodeLayer->setGraph(graph);
}

void EditorView::doLayout(xui::Rect frame) {
    setFrame(frame);
    nodeLayer->layout({ {}, frame.size() });
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
    doLayout(frame());
}
