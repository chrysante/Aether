#include "Aether/DrawingContext.h"

using namespace xui;
using namespace vml::short_types;

void DrawingContext::addDrawCall(DrawCall dc) {
    if (dc.beginVertex == dc.endVertex || dc.beginIndex == dc.endIndex) {
        return;
    }
    drawCalls.push_back(dc);
}

auto DrawingContext::vertexEmitter() {
    return [this](float2 p) { addVertex(p); };
}

auto DrawingContext::triangleEmitter() {
    return [this](uint32_t a, uint32_t b, uint32_t c) { addTriangle(a, b, c); };
}

void DrawingContext::addLine(std::span<vml::float2 const> points,
                             LineMeshOptions const& options) {
    recordDrawCall([&] {
        buildLineMesh(points, vertexEmitter(), triangleEmitter(), options);
    });
}

void DrawingContext::addPolygon(std::span<vml::float2 const> points) {
    recordDrawCall([&] {
        std::ranges::copy(points, std::back_inserter(vertices));
        triangulatePolygon(points, triangleEmitter());
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
