#ifndef AETHER_SHAPES_H
#define AETHER_SHAPES_H

#include <ranges>
#include <type_traits>
#include <vector>

#include <Aether/Vec.h>

namespace xui {

struct LineCapOptions {
    enum Style { None, Circle };

    Style style = None;
    int numSegments = 0;
};

struct LineMeshOptions {
    float width = 10;
    bool closed = false;
    LineCapOptions beginCap, endCap;
};

template <std::unsigned_integral IndexType = uint32_t, typename V = void,
          std::random_access_iterator Itr, std::sentinel_for<Itr> S,
          typename VertexType = std::conditional_t<std::is_same_v<V, void>,
                                                   std::iter_value_t<Itr>, V>,
          std::output_iterator<VertexType> VertexOut,
          std::output_iterator<IndexType> IndexOut>
void buildLineMesh(Itr begin, S end, VertexOut vertexOut, IndexOut indexOut,
                   LineMeshOptions options) {
    if (begin == end) {
        return;
    }
    auto rotate = [](VertexType v) { return VertexType(v.y, -v.x); };
    auto emitQuadIndices = [&](IndexType a, IndexType b, IndexType c,
                               IndexType d) {
        // a -- b
        // |  / |
        // | /  |
        // c -- d

        // Triangle 1
        *indexOut++ = b;
        *indexOut++ = a;
        *indexOut++ = c;
        // Triangle 2
        *indexOut++ = b;
        *indexOut++ = c;
        *indexOut++ = d;
    };
    VertexType lastSegNormal = [&] {
        if (options.closed) {
            auto last = begin + std::ranges::distance(begin, end) - 1;
            auto tangent = normalize(*begin - *last);
            return rotate(tangent);
        }
        else {
            return VertexType{ 0, 0 };
        }
    }();
    IndexType index = 0;
    auto emitVertices = [&](auto point, auto tangent) {
        VertexType segNormal = rotate(tangent);
        VertexType normal = normalize(segNormal + lastSegNormal);
        auto cosAlpha = dot(segNormal, normal);
        auto normalLen = (options.width / 2) / cosAlpha;
        VertexType offset = normalLen * normal;
        lastSegNormal = segNormal;
        *vertexOut++ = point + offset;
        *vertexOut++ = point - offset;
        index += 2;
    };
    auto i = begin;
    while (true) {
        auto next = std::next(i);
        if (next == end) {
            break;
        }
        emitQuadIndices(index, index + 1, index + 2, index + 3);
        emitVertices(*i, normalize(*next - *i));
        i = next;
    }
    if (options.closed) {
        emitQuadIndices(index, index + 1, 0, 1);
        emitVertices(*i, normalize(*begin - *i));
        return;
    }
    emitVertices(*i, normalize(*i - *(i - 1)));
    // TODO: Generate caps
}

} // namespace xui

#endif // AETHER_SHAPES_H
