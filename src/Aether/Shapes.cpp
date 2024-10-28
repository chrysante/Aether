#include "Aether/Shapes.h"

#include <algorithm>
#include <alloca.h>
#include <cassert>
#include <concepts>
#include <iostream>
#include <ranges>
#include <type_traits>
#include <vector>

#include <utl/stack.hpp>

using namespace xui;

template <typename FloatType = float, std::random_access_iterator Itr,
          std::sentinel_for<Itr> S,
          std::invocable<vml::vector2<FloatType>> VertexEmitter>
static void pathBezierImpl(Itr begin, S end, VertexEmitter vertexEmitter,
                           BezierOptions options) {
    using VecType = vml::vector2<FloatType>;
    size_t count = std::distance(begin, end);
    VecType* data = (VecType*)alloca(count * sizeof(VecType));
    int numIterations = options.numSegments - (options.emitLastPoint ? 0 : 1);
    for (int s = options.emitFirstPoint ? 0 : 1; s <= numIterations; ++s) {
        FloatType t = FloatType(s) / options.numSegments;
        std::ranges::copy(begin, end, data);
        for (size_t j = 1; j < count; ++j) {
            for (size_t i = 0; i < count - j; ++i) {
                data[i] = (1 - t) * data[i] + t * data[i + 1];
            }
        }
        std::invoke(vertexEmitter, data[0]);
    }
}

void xui::pathBezier(std::span<vml::float2 const> controlPoints,
                     utl::function_view<void(vml::float2)> vertexEmitter,
                     BezierOptions options) {
    return pathBezierImpl(controlPoints.begin(), controlPoints.end(),
                          vertexEmitter, options);
}

template <typename FloatType = float,
          std::invocable<vml::vector2<FloatType>> VertexEmitter>
static void pathCircleSegmentImpl(vml::vector2<FloatType> begin,
                                  vml::vector2<FloatType> origin,
                                  FloatType totalAngle,
                                  VertexEmitter vertexEmitter,
                                  CircleSegmentOptions options = {}) {
    int end = options.numSegments - (options.emitLast ? 0 : 1);
    int orientation = options.orientation == Orientation::Counterclockwise ? 1 :
                                                                             -1;
    auto v = begin - origin;
    for (int i = options.emitFirst ? 0 : 1; i <= end; ++i) {
        FloatType angle =
            orientation * totalAngle * FloatType(i) / options.numSegments;
        std::invoke(vertexEmitter, origin + vml::rotate(v, angle));
    }
}

void xui::pathCircleSegment(vml::float2 begin, vml::float2 origin,
                            float totalAngle,
                            utl::function_view<void(vml::float2)> vertexEmitter,
                            CircleSegmentOptions options) {
    pathCircleSegmentImpl(begin, origin, totalAngle, vertexEmitter, options);
}

template <std::unsigned_integral IndexType = uint32_t,
          typename FloatType = float, std::random_access_iterator Itr,
          std::sentinel_for<Itr> S, std::invocable<vml::float2> VertexEmitter,
          std::invocable<IndexType, IndexType, IndexType> TriangleEmitter>
static void buildLineMeshImpl(Itr begin, S end, VertexEmitter vertexEmitter,
                              TriangleEmitter triangleEmitter,
                              LineMeshOptions options) {
    using VecType = vml::vector2<FloatType>;
    if (begin == end) {
        return;
    }
    size_t numPoints = std::ranges::distance(begin, end);
    auto lastItr = begin + numPoints - 1;
    IndexType numVertices = 0;
    auto emitVertex = [&vertexEmitter, &numVertices](VecType position,
                                                     size_t index) {
        if constexpr (std::invocable<VertexEmitter, VecType, size_t>) {
            std::invoke(vertexEmitter, position, index);
        }
        else {
            std::invoke(vertexEmitter, position);
        }
        ++numVertices;
    };
    auto emitTriangle = [&triangleEmitter](IndexType a, IndexType b,
                                           IndexType c) {
        std::invoke(triangleEmitter, a, b, c);
    };
    auto rotate = [](vml::float2 v) { return VecType(v.y, -v.x); };
    auto emitQuad = [&](IndexType a, IndexType b, IndexType c, IndexType d) {
        // a -- b
        // |  / |
        // | /  |
        // c -- d
        emitTriangle(b, a, c);
        emitTriangle(b, c, d);
    };
    VecType lastSegNormal = [&] {
        if (options.closed) {
            auto tangent = normalize(*begin - *lastItr);
            return rotate(tangent);
        }
        else {
            return VecType{ 0, 0 };
        }
    }();
    auto emitVertices = [&](size_t pointIndex, auto point, auto tangent) {
        VecType segNormal = rotate(tangent);
        VecType normal = normalize(segNormal + lastSegNormal);
        auto cosAlpha = dot(segNormal, normal);
        auto normalLen = (options.width / 2) / cosAlpha;
        VecType offset = normalLen * normal;
        lastSegNormal = segNormal;
        emitVertex(point + offset, pointIndex);
        emitVertex(point - offset, pointIndex);
    };
    auto i = begin;
    size_t pointIndex = 0;
    while (true) {
        auto next = std::next(i);
        if (next == end) {
            break;
        }
        emitQuad(numVertices, numVertices + 1, numVertices + 2,
                 numVertices + 3);
        emitVertices(pointIndex, *i, normalize(*next - *i));
        i = next;
        ++pointIndex;
    }
    if (options.closed) {
        emitQuad(numVertices, numVertices + 1, 0, 1);
        emitVertices(pointIndex, *i, normalize(*begin - *i));
        return;
    }
    emitVertices(pointIndex, *i, normalize(*i - *(i - 1)));
    auto generateCap = [&](LineCapOptions const& cap, size_t pointIndex,
                           VecType point, VecType tangent, IndexType idxA,
                           IndexType idxB) {
        switch (cap.style) {
        case LineCapOptions::None:
            return;
        case LineCapOptions::Circle:
            emitTriangle(idxA, idxB, numVertices);
            for (int i = 0; i < cap.numSegments; ++i) {
                FloatType angle =
                    M_PI * (FloatType(i + 1) / (cap.numSegments + 1) - 0.5);
                auto mat = vml::make_rotation2x2(angle);
                emitVertex(point + options.width / 2 * (mat * tangent),
                           pointIndex);
                if (i != cap.numSegments - 1) {
                    emitTriangle(numVertices - 1, idxB, numVertices);
                }
            }
            return;
        }
    };
    IndexType numSegmentVertices = numVertices;
    generateCap(options.beginCap, 0, *begin,
                normalize(*begin - *std::next(begin)), 0, 1);
    generateCap(options.endCap, numPoints, *lastItr,
                normalize(*lastItr - *(lastItr - 1)), numSegmentVertices - 1,
                numSegmentVertices - 2);
}

void xui::buildLineMesh(
    std::span<vml::float2 const> line,
    utl::function_view<void(vml::float2)> vertexEmitter,
    utl::function_view<void(uint32_t, uint32_t, uint32_t)> triangleEmitter,
    LineMeshOptions options) {
    buildLineMeshImpl(line.begin(), line.end(), vertexEmitter, triangleEmitter,
                      options);
}

template <typename VecType>
static auto signedArea(VecType p1, VecType p2, VecType p3) {
    return (p2.x - p1.x) * (p3.y - p1.y) - (p2.y - p1.y) * (p3.x - p1.x);
}

template <typename VecType>
static bool isPointInTriangle(VecType p, VecType a, VecType b, VecType c) {
    auto area1 = signedArea(p, a, b);
    auto area2 = signedArea(p, b, c);
    auto area3 = signedArea(p, c, a);
    return (area1 > 0 && area2 > 0 && area3 > 0) ||
           (area1 < 0 && area2 < 0 && area3 < 0);
}

template <typename VecType>
static bool isConvex(VecType prev, VecType curr, VecType next) {
    return signedArea(prev, curr, next) > 0;
}

static bool isEar(auto begin, auto end, size_t prev, size_t curr, size_t next) {
    if (!isConvex(*(begin + prev), *(begin + curr), *(begin + next))) {
        return false;
    }
    size_t i = 0;
    for (auto itr = begin; itr != end; ++itr, ++i) {
        if (i == prev || i == curr || i == next) {
            continue;
        }
        if (isPointInTriangle(*(begin + i), *(begin + prev), *(begin + curr),
                              *(begin + next)))
        {
            return false;
        }
    }
    return true;
}

template <typename IndexType>
static std::vector<IndexType> makeIndexVector(size_t vertexCount) {
    std::vector<IndexType> indices;
    indices.reserve(vertexCount);
    for (IndexType i = 0; i < vertexCount; ++i) {
        indices.push_back(i);
    }
    return indices;
}

template <std::unsigned_integral IndexType = uint32_t, std::input_iterator Itr,
          std::sentinel_for<Itr> S,
          std::invocable<IndexType, IndexType, IndexType> TriangleEmitter>
void static triangulatePolyEarClipping(Itr begin, S end,
                                       TriangleEmitter triangleEmitter) {
    size_t vertexCount = std::ranges::distance(begin, end);
    std::vector<IndexType> vertexIndices =
        makeIndexVector<IndexType>(vertexCount);
loopBegin:
    while (vertexIndices.size() > 3) {
        size_t n = vertexIndices.size();
        for (size_t i = 0; i < n; ++i) {
            IndexType prev = vertexIndices[(i + n - 1) % n];
            IndexType curr = vertexIndices[i];
            IndexType next = vertexIndices[(i + 1) % n];
            if (isEar(begin, end, prev, curr, next)) {
                std::invoke(triangleEmitter, prev, curr, next);
                vertexIndices.erase(vertexIndices.begin() + i);
                goto loopBegin;
            }
        }
        std::cerr << "No ear found, possibly degenerate polygon\n";
        return;
    }
    std::invoke(triangleEmitter, vertexIndices[0], vertexIndices[1],
                vertexIndices[2]);
}

template <std::unsigned_integral IndexType>
bool is_distance_one_mod_n(IndexType a, IndexType b, size_t n) {
    IndexType diff = a > b ? a - b : b - a;
    return diff == 1 || diff == n - 1;
}

template <std::unsigned_integral IndexType = uint32_t, std::input_iterator Itr,
          std::sentinel_for<Itr> S,
          std::invocable<IndexType, IndexType, IndexType> TriangleEmitter>
void static triangulatePolyYMonotone(Itr begin, S end,
                                     TriangleEmitter triangleEmitter,
                                     Orientation orientation) {
    size_t vertexCount = std::ranges::distance(begin, end);
    assert(vertexCount >= 3);
    std::vector<IndexType> vertexIndices =
        makeIndexVector<IndexType>(vertexCount);
    auto vertexAt = [begin](IndexType index) -> auto& {
        return *(begin + index);
    };
    auto isConvex = [&](IndexType a, IndexType b, IndexType c) -> bool {
        auto p1 = vertexAt(a);
        auto p2 = vertexAt(b);
        auto p3 = vertexAt(c);
        return (p2.x - p1.x) * (p3.y - p1.y) - (p2.y - p1.y) * (p3.x - p1.x) >
               0;
    };
    auto compare = [&](IndexType i, IndexType j) {
        auto a = vertexAt(i);
        auto b = vertexAt(j);
        return a.y == b.y ? a.x < b.x : a.y < b.y;
    };
    std::ranges::sort(vertexIndices, compare);
    utl::stack<IndexType> stack = { vertexIndices[0], vertexIndices[1] };
    size_t indexIndex = 2;
    bool isLeft = (vertexIndices[1] == (vertexIndices[0] + 1) % vertexCount) ==
                  (orientation == Orientation::Counterclockwise);
    for (IndexType index: vertexIndices | std::views::drop(2)) {
        IndexType prev = stack.pop();
        bool sameChain = is_distance_one_mod_n(index, prev, vertexCount);
        if (sameChain) {
            while (!stack.empty() &&
                   (index == vertexIndices.back() ||
                    isLeft != isConvex(stack.top(), prev, index)))
            {
                IndexType top = stack.pop();
                std::invoke(triangleEmitter, index, top, prev);
                prev = top;
            }
            stack.push(prev);
            stack.push(index);
        }
        else {
            isLeft = !isLeft;
            while (!stack.empty()) {
                IndexType top = stack.pop();
                std::invoke(triangleEmitter, index, top, prev);
                prev = top;
            }
            stack.push(vertexIndices[indexIndex - 1]);
            stack.push(index);
        }
        ++indexIndex;
    }
}

void xui::triangulatePolygon(
    std::span<vml::float2 const> vertices,
    utl::function_view<void(uint32_t, uint32_t, uint32_t)> triangleEmitter,
    TriangulationOptions options) {
    if (options.isYMonotone) {
        triangulatePolyYMonotone(vertices.begin(), vertices.end(),
                                 triangleEmitter, options.orientation);
        return;
    }
    triangulatePolyEarClipping(vertices.begin(), vertices.end(),
                               triangleEmitter);
}
