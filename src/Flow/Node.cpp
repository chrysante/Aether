#include "Flow/Node.h"

using namespace flow;

void flow::link(OutputPin& source, InputPin& sink) {
    source.addUser(&sink);
    if (sink.source()) sink.source()->removeUser(&sink);
    sink.setSource(&source);
}

void flow::link(Pin& a, Pin& b) {
    // clang-format off
    visit(a, b, csp::overload{
        [](InputPin& a, OutputPin& b) { link(a, b); },
        [](OutputPin& a, InputPin& b) { link(b, a); },
        [](Pin const&, Pin const&) {},
    }); // clang-format on
}
