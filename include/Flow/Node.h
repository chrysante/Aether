#ifndef Node_h
#define Node_h

#include <memory>
#include <ranges>
#include <span>

#include <Aether/ADT.h>
#include <utl/ilist.hpp>
#include <utl/vector.hpp>

namespace flow {

class Node;
class InputPin;
class OutputPin;

///
struct PinDesc {
    std::string label;
    bool optional = false;
};

/// Base class of `InputPin` and `OutputPin`
class Pin {
public:
    Pin(Pin const&) = delete;
    Pin& operator=(Pin const&) = delete;

    /// \Returns the owning node
    Node* node() const { return _node; }

    /// \Returns the description
    PinDesc const& desc() const { return _desc; }

    /// \Returns the label
    std::string const& label() const { return _desc.label; }

protected:
    explicit Pin(Node* node, PinDesc desc):
        _node(node), _desc(std::move(desc)) {}

private:
    Node* _node;
    PinDesc _desc;
};

/// Models an input to a node
class InputPin: public Pin {
public:
    explicit InputPin(Node* node, PinDesc desc): Pin(node, std::move(desc)) {}

    /// \Returns the source pin
    OutputPin* source() const { return _source; }

    /// Sets the source pin
    void setSource(OutputPin* source) { _source = source; }

private:
    OutputPin* _source = nullptr;
};

/// Models an output of a node
class OutputPin: public Pin {
public:
    explicit OutputPin(Node* node, PinDesc desc): Pin(node, std::move(desc)) {}

    /// \Returns a view over the users of this output, i.e., input pins of other
    /// nodes
    std::span<InputPin* const> users() const { return _users; }

    /// Adds \p user as a user of this output
    void addUser(InputPin* user) { _users.push_back(user); }

    /// Removes \p user
    void removeUser(InputPin* user) {
        auto itr = std::ranges::find(_users, user);
        if (itr != _users.end()) _users.erase(itr);
    }

private:
    utl::small_vector<InputPin*> _users;
};

///
struct NodeDesc {
    std::string name;
    xui::Point position;
    utl::small_vector<PinDesc> inputs, outputs;
};

///
class Node: public utl::ilist_node<Node> {
public:
    explicit Node(NodeDesc desc):
        _name(std::move(desc.name)), _position(desc.position) {
        for (auto& pinDesc: desc.inputs)
            addInput(std::move(pinDesc));
        for (auto& pinDesc: desc.outputs)
            addOutput(std::move(pinDesc));
    }

    Node(Node const&) = delete;
    Node& operator=(Node const&) = delete;

    /// \Returns the name
    std::string const& name() const { return _name; }

    /// \Return the position relative to other nodes
    xui::Point position() const { return _position; }

    ///
    void setPosition(xui::Point position) { _position = position; }

    /// Creates an input pin on this node using \p desc
    InputPin* addInput(PinDesc desc) {
        _inputs.push_back(std::make_unique<InputPin>(this, std::move(desc)));
        return _inputs.back().get();
    }

    /// Creates an output pin on this node using \p desc
    OutputPin* addOutput(PinDesc desc) {
        _outputs.push_back(std::make_unique<OutputPin>(this, std::move(desc)));
        return _outputs.back().get();
    }

    /// \Returns a view over the input pins
    auto inputs() const {
        return _inputs | std::views::transform(xui::detail::Get);
    }

    /// \Returns a view over the output pins
    auto outputs() const {
        return _outputs | std::views::transform(xui::detail::Get);
    }

    /// \Returns the successor nodes
    auto successors() const {
        return outputs() | std::views::transform([](OutputPin const* pin) {
            return pin->users() | std::views::transform(&Pin::node);
        }) | std::views::join;
    }

    /// \Returns the predecessor nodes
    auto predecessors() const {
        return inputs() | std::views::transform([](InputPin const* pin) {
            return pin->source()->node();
        });
    }

private:
    std::string _name;
    xui::Point _position;
    utl::small_vector<std::unique_ptr<InputPin>> _inputs;
    utl::small_vector<std::unique_ptr<OutputPin>> _outputs;
};

} // namespace flow

#endif /* Node_h */
