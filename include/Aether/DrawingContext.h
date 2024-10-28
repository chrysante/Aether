#ifndef AETHER_DRAWINGCONTEXT_H
#define AETHER_DRAWINGCONTEXT_H

#include <span>
#include <vector>

#include <vml/vml.hpp>

#include <Aether/Shapes.h>
#include <Aether/Vec.h>

namespace xui {

class View;

struct DrawCallOptions {
    Color color = Color::Red();
    bool wireframe = false;
};

struct DrawCall {
    size_t beginVertex, endVertex;
    size_t beginIndex, endIndex;
    DrawCallOptions options = {};
};

/// Platform independent renderer interface
class Renderer {
public:
    virtual ~Renderer() = default;

    virtual void render(std::span<vml::float2 const> vertices,
                        std::span<uint32_t const> indices,
                        std::span<DrawCall const> drawCalls) = 0;
};

/// Creates a platform dependent implementation of `Renderer`
std::unique_ptr<Renderer> createRenderer(View* view);

/// Wrapper around a `Renderer` that provides convenience drawing functions
class DrawingContext {
public:
    explicit DrawingContext(View* view): renderer(createRenderer(view)) {}

    /// Creates a draw call that draws \p line
    void addLine(std::span<vml::float2 const> line,
                 DrawCallOptions const& drawOptions = {},
                 LineMeshOptions const& meshOptions = {});

    /// Creates a draw call that draws \p polygon
    void addPolygon(std::span<vml::float2 const> polygon,
                    DrawCallOptions const& drawOptions = {},
                    TriangulationOptions const& meshOptions = {});

    /// Stateful rendering interface @{

    /// Invokes \p fn between a call to `beginDrawCall()` and `endDrawCall()`
    void recordDrawCall(std::invocable auto&& fn) {
        recordDrawCall({}, std::forward<decltype(fn)>(fn));
    }

    /// \overload
    void recordDrawCall(DrawCallOptions options, std::invocable auto&& fn) {
        beginDrawCall(options);
        std::invoke(fn);
        endDrawCall();
    }

    /// Starts recording a draw call
    void beginDrawCall(DrawCallOptions options = {}) {
        currentDC = { .beginVertex = vertices.size(),
                      .beginIndex = indices.size(),
                      .options = options };
    }

    /// Adds a vertex to the currently recording draw call
    void addVertex(vml::float2 p) { vertices.push_back(p); }

    ///
    void addTriangle(uint32_t a, uint32_t b, uint32_t c) {
        indices.push_back(a);
        indices.push_back(b);
        indices.push_back(c);
    }

    /// Ends recording a draw call
    void endDrawCall() {
        currentDC.endVertex = vertices.size();
        currentDC.endIndex = indices.size();
        addDrawCall(currentDC);
    }

    /// \Returns a function that adds vertices to the current draw call
    auto vertexEmitter() {
        return [this](vml::float2 p) { addVertex(p); };
    }

    /// \Returns a function that adds triangles to the current draw call
    auto triangleEmitter() {
        return [this](uint32_t a, uint32_t b, uint32_t c) {
            addTriangle(a, b, c);
        };
    }

    /// @}

    /// Draws the recorded draw calls
    void draw();

    /// Returns the underlying renderer
    Renderer* getRenderer() { return renderer.get(); }

private:
    void addDrawCall(DrawCall drawCall);

    DrawCall currentDC{};
    std::unique_ptr<Renderer> renderer;
    std::vector<vml::float2> vertices;
    std::vector<uint32_t> indices;
    std::vector<DrawCall> drawCalls;
};

} // namespace xui

#endif // AETHER_DRAWINGCONTEXT_H
