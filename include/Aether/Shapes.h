#ifndef AETHER_SHAPES_H
#define AETHER_SHAPES_H

#include <algorithm>
#include <concepts>
#include <iostream>
#include <ranges>
#include <span>
#include <type_traits>
#include <vector>

#include <alloca.h>
#include <vml/vml.hpp>

namespace xui {

struct BezierOptions {
    int numSegments = 30;
    bool emitFirstPoint = true;
    bool emitLastPoint = true;
};

template <typename FloatType = float, std::random_access_iterator Itr,
          std::sentinel_for<Itr> S,
          std::invocable<vml::vector2<FloatType>> VertexEmitter>
void pathBezier(Itr begin, S end, VertexEmitter vertexEmitter,
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
        vertexEmitter(data[0]);
    }
}

struct CircleSegmentOptions {
    enum Orientation { CCW = 0, CW = 1 };
    Orientation orientation = CCW;
    int numSegments = 20;
    bool emitFirst = true;
    bool emitLast = true;
};

template <typename FloatType = float,
          std::invocable<vml::vector2<FloatType>> VertexEmitter>
void pathCircleSegment(vml::vector2<FloatType> begin,
                       vml::vector2<FloatType> origin, FloatType totalAngle,
                       VertexEmitter vertexEmitter,
                       CircleSegmentOptions options = {}) {
    int end = options.numSegments - (options.emitLast ? 0 : 1);
    int orientation = options.orientation == CircleSegmentOptions::CCW ? 1 : -1;
    auto v = begin - origin;
    for (int i = options.emitFirst ? 0 : 1; i <= end; ++i) {
        FloatType angle =
            orientation * totalAngle * FloatType(i) / options.numSegments;
        vertexEmitter(origin + vml::rotate(v, angle));
    }
}

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

template <std::unsigned_integral IndexType = uint32_t,
          typename FloatType = float, std::random_access_iterator Itr,
          std::sentinel_for<Itr> S, std::invocable<vml::float2> VertexEmitter,
          std::invocable<IndexType> IndexEmitter>
void buildLineMesh(Itr begin, S end, VertexEmitter vertexEmitter,
                   IndexEmitter indexEmitter, LineMeshOptions options) {
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
    auto emitIndex = [&indexEmitter](IndexType index) {
        std::invoke(indexEmitter, index);
    };
    auto rotate = [](vml::float2 v) { return VecType(v.y, -v.x); };
    auto emitQuadIndices = [&](IndexType a, IndexType b, IndexType c,
                               IndexType d) {
        // a -- b
        // |  / |
        // | /  |
        // c -- d
        // Triangle 1
        emitIndex(b);
        emitIndex(a);
        emitIndex(c);
        // Triangle 2
        emitIndex(b);
        emitIndex(c);
        emitIndex(d);
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
        emitQuadIndices(numVertices, numVertices + 1, numVertices + 2,
                        numVertices + 3);
        emitVertices(pointIndex, *i, normalize(*next - *i));
        i = next;
        ++pointIndex;
    }
    if (options.closed) {
        emitQuadIndices(numVertices, numVertices + 1, 0, 1);
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
            emitIndex(idxA);
            emitIndex(idxB);
            emitIndex(numVertices);
            for (int i = 0; i < cap.numSegments; ++i) {
                FloatType angle =
                    M_PI * (FloatType(i + 1) / (cap.numSegments + 1) - 0.5);
                auto mat = vml::make_rotation2x2(angle);
                emitVertex(point + options.width / 2 * (mat * tangent),
                           pointIndex);
                if (i != cap.numSegments - 1) {
                    emitIndex(numVertices - 1);
                    emitIndex(idxB);
                    emitIndex(numVertices);
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

namespace detail {

template <typename VecType>
auto signedArea(VecType p1, VecType p2, VecType p3) {
    return (p2.x - p1.x) * (p3.y - p1.y) - (p2.y - p1.y) * (p3.x - p1.x);
}

template <typename VecType>
bool isPointInTriangle(VecType p, VecType a, VecType b, VecType c) {
    auto area1 = signedArea(p, a, b);
    auto area2 = signedArea(p, b, c);
    auto area3 = signedArea(p, c, a);
    return (area1 > 0 && area2 > 0 && area3 > 0) ||
           (area1 < 0 && area2 < 0 && area3 < 0);
}

template <typename VecType>
bool isConvex(VecType prev, VecType curr, VecType next) {
    return signedArea(prev, curr, next) > 0;
}

bool isEar(auto begin, auto end, size_t prev, size_t curr, size_t next) {
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

} // namespace detail

template <std::unsigned_integral IndexType = uint32_t, std::input_iterator Itr,
          std::sentinel_for<Itr> S, std::invocable<IndexType> IndexEmitter>
void triangulatePolygon(Itr begin, S end, IndexEmitter indexEmitter) {
    using namespace detail;
    size_t vertexCount = std::ranges::distance(begin, end);
    std::vector<IndexType> vertexIndices;
    vertexIndices.reserve(vertexCount);
    for (IndexType i = 0; i < vertexCount; ++i) {
        vertexIndices.push_back(i);
    }
    while (vertexIndices.size() > 3) {
        size_t n = vertexIndices.size();
        for (size_t i = 0; i < n; ++i) {
            IndexType prev = vertexIndices[(i + n - 1) % n];
            IndexType curr = vertexIndices[i];
            IndexType next = vertexIndices[(i + 1) % n];
            if (isEar(begin, end, prev, curr, next)) {
                indexEmitter(prev);
                indexEmitter(curr);
                indexEmitter(next);
                vertexIndices.erase(vertexIndices.begin() + i);
                goto end;
            }
        }
        std::cerr << "No ear found, possibly degenerate polygon\n";
        return;
end:
        (void)0;
    }
    indexEmitter(vertexIndices[0]);
    indexEmitter(vertexIndices[1]);
    indexEmitter(vertexIndices[2]);
}

} // namespace xui

#endif // AETHER_SHAPES_H
