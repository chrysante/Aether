#ifndef AETHER_SHAPES_H
#define AETHER_SHAPES_H

#include <span>

#include <utl/function_view.hpp>
#include <vml/vml.hpp>

namespace xui {

enum class Orientation { Counterclockwise, Clockwise };

struct BezierOptions {
    int numSegments = 30;
    bool emitFirstPoint = true;
    bool emitLastPoint = true;
};

void pathBezier(std::span<vml::float2 const> controlPoints,
                utl::function_view<void(vml::float2)> vertexEmitter,
                BezierOptions options = {});

struct CircleSegmentOptions {
    Orientation orientation = Orientation::Counterclockwise;
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

struct TriangulationOptions {
    /// Y monotone polygons are polygons for which each horizonal line only
    /// intersects the polygon atmost twice. Y monotone polygons can be
    /// triangulated in linear time
    bool isYMonotone = false;

    /// Required when `isYMonotone` is true
    Orientation orientation = {};
};

void triangulatePolygon(
    std::span<vml::float2 const> vertices,
    utl::function_view<void(uint32_t, uint32_t, uint32_t)> triangleEmitter,
    TriangulationOptions options = {});

} // namespace xui

#endif // AETHER_SHAPES_H
