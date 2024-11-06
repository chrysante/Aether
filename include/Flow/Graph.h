#ifndef FLOW_GRAPH_H
#define FLOW_GRAPH_H

#include <memory>
#include <ranges>

#include <Aether/ADT.h>
#include <utl/ilist.hpp>

#include <Flow/Node.h>

namespace flow {

///
class Graph {
public:
    /// Adds \p node to the graph
    Node* addNode(std::unique_ptr<Node> node) {
        auto* ptr = node.release();
        _nodes.push_back(ptr);
        return ptr;
    }

    /// \overload
    Node* addNode(NodeDesc desc) {
        return addNode(std::make_unique<Node>(std::move(desc)));
    }

    /// Removes \p node from the graph
    void eraseNode(Node* node) { _nodes.erase(node); }

    /// \Returns a view over the nodes
    auto nodes() {
        return _nodes | std::views::transform(xui::detail::AddressOf);
    }

    /// \return
    auto nodes() const {
        return _nodes |
               std::views::transform(xui::detail::AddressOfT<Node const*>);
    }

private:
    utl::ilist<Node> _nodes;
};

} // namespace flow

#endif // FLOW_GRAPH_H
