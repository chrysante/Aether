#ifndef FLOW_EDITOR_H
#define FLOW_EDITOR_H

#include <Aether/View.h>

namespace flow {

class Graph;
class NodeLayerView;
class SelectionLayerView;

///
class EditorView: public xui::View {
public:
    explicit EditorView(Graph* graph = nullptr);

    void setGraph(Graph* graph);

    Graph* graph() const { return _graph; }

    xui::Point surfaceOrigin() const { return _origin; }

private:
    void doLayout(xui::Rect frame) override;
    bool onEvent(xui::ScrollEvent const&) override;
    bool onEvent(xui::MouseDownEvent const&) override;
    bool onEvent(xui::MouseUpEvent const&) override;
    bool onEvent(xui::MouseDragEvent const&) override;

    void addOriginDelta(xui::Vec2<double> delta);

    xui::Point _origin{};
    Graph* _graph;
    NodeLayerView* nodeLayer;
    SelectionLayerView* selectionLayer;
};

} // namespace flow

#endif // FLOW_EDITOR_H
