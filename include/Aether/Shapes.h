#ifndef AETHER_SHAPES_H
#define AETHER_SHAPES_H

#include <span>

#include <utl/function_view.hpp>
#include <vml/vml.hpp>

namespace xui {

struct BezierOptions {
    int numSegments = 30;
    bool emitFirstPoint = true;
    bool emitLastPoint = true;
};

void pathBezier(std::span<vml::float2 const> controlPoints,
                utl::function_view<void(vml::float2)> vertexEmitter,
                BezierOptions options = {});

struct CircleSegmentOptions {
    enum Orientation { CCW = 0, CW = 1 };
    Orientation orientation = CCW;
    int numSegments = 20;
    bool emitFirst = true;
    bool emitLast = true;
};

void pathCircleSegment(vml::float2 begin, vml::float2 origin, float totalAngle,
                       utl::function_view<void(vml::float2)> vertexEmitter,
                       CircleSegmentOptions options = {});

struct LineCapOptions {
    enum Style { None, Circle };

    Style style = None;
    int numSegments = 20;
};

struct LineMeshOptions {
    float width = 10;
    bool closed = false;
    LineCapOptions beginCap, endCap;
};

void buildLineMesh(
    std::span<vml::float2 const> line,
    utl::function_view<void(vml::float2)> vertexEmitter,
    utl::function_view<void(uint32_t, uint32_t, uint32_t)> triangleEmitter,
    LineMeshOptions options);

void triangulatePolygon(
    std::span<vml::float2 const> vertices,
    utl::function_view<void(uint32_t, uint32_t, uint32_t)> triangleEmitter);

} // namespace xui

#endif // AETHER_SHAPES_H
