#include <iostream>

#include <Aether/Application.h>
#include <Aether/Modifiers.h>
#include <Aether/Toolbar.h>
#include <Aether/View.h>
#include <Aether/Window.h>
#include <Flow/Editor.h>
#include <Flow/Graph.h>

using namespace xui;

namespace {

struct Sandbox: Application {
    Sandbox() {
        auto* A = graph.addNode(
            { .name = "My Node", .inputs = { {} }, .outputs = { {} } });
        auto* B = graph.addNode(
            { .name = "Other Node", .inputs = { {} }, .outputs = { {} } });
        link(A->output(0), B->input(0));
        createWindow()->setContentView(NodeEditor());
    }

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
        return HSplit(
                   { Sidebar(), std::make_unique<flow::EditorView>(&graph) }) |
               SplitViewResizeStrategy::CutRight;
    }

    flow::Graph graph;
    std::vector<std::unique_ptr<Window>> windows;
};

} // namespace

std::unique_ptr<Application> xui::createApplication() {
    return std::make_unique<Sandbox>();
}
