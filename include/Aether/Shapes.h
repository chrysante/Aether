#ifndef AETHER_SHAPES_H
#define AETHER_SHAPES_H

#include <concepts>
#include <ranges>
#include <type_traits>
#include <vector>

#include <vml/vml.hpp>

namespace xui {

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
    using VecType = vml::vector<FloatType, 2>;
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

    auto rotMatrix = [](FloatType angle) {
        return vml::float2x2{ std::cos(angle), std::sin(angle),
                              -std::sin(angle), std::cos(angle) };
    };

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
                auto mat = rotMatrix(angle);
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

} // namespace xui

#endif // AETHER_SHAPES_H
