#include "Aether/DrawingContext.h"

using namespace xui;
using namespace vml::short_types;

void DrawingContext::addDrawCall(DrawCall dc) {
    if (dc.beginVertex == dc.endVertex || dc.beginIndex == dc.endIndex) {
        return;
    }
    drawCalls.push_back(dc);
}

void DrawingContext::addLine(std::span<vml::float2 const> points,
                             DrawCallOptions const& drawOptions,
                             LineMeshOptions const& meshOptions) {
    recordDrawCall(drawOptions, [&] {
        buildLineMesh(points, vertexEmitter(), triangleEmitter(), meshOptions);
    });
}

void DrawingContext::addPolygon(std::span<vml::float2 const> points,
                                DrawCallOptions const& drawOptions,
                                TriangulationOptions const& meshOptions) {
    recordDrawCall(drawOptions, [&] {
        std::ranges::copy(points, std::back_inserter(vertices));
        triangulatePolygon(points, triangleEmitter(), meshOptions);
    });
}

void DrawingContext::draw() {
    if (renderer) {
        renderer->render(vertices, indices, drawCalls);
    }
    vertices.clear();
    indices.clear();
    drawCalls.clear();
}
