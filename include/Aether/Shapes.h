#ifndef AETHER_SHAPES_H
#define AETHER_SHAPES_H

#include <ranges>
#include <type_traits>
#include <vector>

#include <Aether/Vec.h>

namespace xui {

struct LineCapStyle {
    enum Kind { None, Circle };

    Kind kind = None;
    int numSegments = 0;
};

struct LineMeshOptions {
    float width = 10;
    bool closed = false;
    LineCapStyle beginCap, endCap;
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
    auto makeRot = [](auto a, auto b) {
        auto diff = b - a;
        return normalize(VertexType(diff.y, -diff.x));
    };
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
    auto i = begin;
    VertexType firstRot = [&] {
        if (options.closed) {
            return makeRot(*(begin + std::ranges::distance(begin, end) - 1),
                           *begin);
        }
        else {
            return VertexType{ 0, 0 };
        }
    }();
    VertexType lastRot = firstRot;
    IndexType index = 0;
    while (true) {
        auto next = std::next(i);
        if (next == end) {
            break;
        }
        VertexType rot = makeRot(*i, *next);
        auto offset = options.width * normalize(rot + lastRot);
        lastRot = rot;
        *vertexOut++ = *i + offset;
        *vertexOut++ = *i - offset;
        emitQuadIndices(index, index + 1, index + 2, index + 3);
        index += 2;
        i = next;
    }
    if (options.closed) {
        auto offset = options.width * normalize(lastRot + firstRot);
        *vertexOut++ = *i + offset;
        *vertexOut++ = *i - offset;
        emitQuadIndices(index, index + 1, 0, 1);
    }
    else {
        auto offset = options.width * lastRot;
        *vertexOut++ = *i + offset;
        *vertexOut++ = *i - offset;
    }
}

} // namespace xui

#endif // AETHER_SHAPES_H
