#ifndef AETHER_ADT_H
#define AETHER_ADT_H

#include <algorithm>
#include <memory>
#include <span>
#include <vector>

namespace xui {

struct Position {
    double x, y;
};

struct Size {
    double width, height;
};

struct Rect {
    Position pos;
    Size size;
};

enum class Axis { X, Y, Z };

template <typename T>
struct UniqueVector: std::vector<std::unique_ptr<T>> {
private:
    using Base = std::vector<std::unique_ptr<T>>;

public:
    UniqueVector() = default;
    UniqueVector(Base&& v): Base(std::move(v)) {}

    template <size_t N>
    UniqueVector(std::unique_ptr<T> (&&elems)[N]):
        Base(std::move_iterator(std::begin(elems)),
             std::move_iterator(std::end(elems))) {}
};

template <typename T>
UniqueVector<T> toUniqueVector(std::span<std::unique_ptr<T>> elems) {
    UniqueVector<T> v;
    v.reserve(elems.size());
    std::move(std::begin(elems), std::end(elems), std::back_inserter(v));
    return v;
}

template <typename T, size_t N>
UniqueVector<T> toUniqueVector(std::unique_ptr<T> (&&elems)[N]) {
    return UniqueVector<T>(std::move(elems));
}

} // namespace xui

#endif // AETHER_ADT_H
