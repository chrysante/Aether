#ifndef AETHER_VEC_H
#define AETHER_VEC_H

#include <algorithm>
#include <cassert>
#include <tuple>

namespace xui {

enum class Axis { X, Y, Z };

constexpr Axis flip(Axis A) {
    using enum Axis;
    switch (A) {
    case X:
        return Y;
    case Y:
        return X;
    default:
        return A;
    }
}

namespace detail {

template <typename T, size_t N>
struct VecData;

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

template <typename T>
struct VecData<T, 2> {
    VecData() = default;
    constexpr VecData(T x, T y): data{ x, y } {}
    explicit constexpr VecData(T value): data{ value, value } {}

    union {
        T data[2]{};
        struct {
            T x, y;
        };
    };
};

template <typename T>
struct VecData<T, 3> {
    VecData() = default;
    constexpr VecData(T x, T y, T z): data{ x, y, z } {}
    explicit constexpr VecData(T value): data{ value, value, value } {}

    union {
        T data[3]{};
        struct {
            T x, y, z;
        };
    };
};

template <typename T>
struct VecData<T, 4> {
    VecData() = default;
    constexpr VecData(T x, T y, T z, T w): data{ x, y, z, w } {}
    explicit constexpr VecData(T value): data{ value, value, value, value } {}

    union {
        T data[4]{};
        struct {
            T x, y, z, w;
        };
    };
};

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

} // namespace detail

template <typename T, size_t N>
struct Vec: detail::VecData<T, N> {
private:
    using Base = detail::VecData<T, N>;

public:
    using Base::data;

    template <typename U>
    Vec(Vec<U, N> const& other) {
        for (size_t i = 0; i < N; ++i) {
            (*this)[i] = other[i];
        }
    }

    Vec() = default;

    using Base::Base;

    constexpr explicit Vec(Axis axis, T value): Vec() { (*this)[axis] = value; }

    constexpr T& operator[](size_t i) {
        assert(i < N);
        return data[i];
    }
    constexpr T const& operator[](size_t i) const {
        assert(i < N);
        return data[i];
    }
    constexpr T& operator[](Axis a) { return (*this)[(size_t)a]; }
    constexpr T const& operator[](Axis a) const { return (*this)[(size_t)a]; }

    constexpr T* begin() { return &data[0]; }
    constexpr T const* begin() const { return &data[0]; }
    constexpr T* end() { return &data[N]; }
    constexpr T const* end() const { return &data[N]; }

    static constexpr size_t size() { return N; }

    constexpr Vec& operator+=(Vec const& rhs) { return *this = *this + rhs; }
    constexpr Vec& operator-=(Vec const& rhs) { return *this = *this - rhs; }
    constexpr Vec& operator*=(Vec const& rhs) { return *this = *this * rhs; }
    constexpr Vec& operator*=(T const& rhs) { return *this = *this * rhs; }
    constexpr Vec& operator/=(Vec const& rhs) { return *this = *this / rhs; }
    constexpr Vec& operator/=(T const& rhs) { return *this = *this / rhs; }
};

template <typename T>
using Vec2 = Vec<T, 2>;

template <typename T, size_t N>
constexpr Vec<T, N> operator+(Vec<T, N> const& a, Vec<T, N> const& b) {
    return [&]<size_t... I>(std::index_sequence<I...>) {
        return Vec<T, N>{ (a.data[I] + b.data[I])... };
    }(std::make_index_sequence<N>());
}

template <typename T, size_t N>
constexpr Vec<T, N> operator-(Vec<T, N> const& a, Vec<T, N> const& b) {
    return [&]<size_t... I>(std::index_sequence<I...>) {
        return Vec<T, N>{ (a.data[I] - b.data[I])... };
    }(std::make_index_sequence<N>());
}

template <typename T, size_t N>
constexpr Vec<T, N> operator*(Vec<T, N> const& a, Vec<T, N> const& b) {
    return [&]<size_t... I>(std::index_sequence<I...>) {
        return Vec<T, N>{ (a.data[I] * b.data[I])... };
    }(std::make_index_sequence<N>());
}

template <typename T, size_t N>
constexpr Vec<T, N> operator*(Vec<T, N> const& a, T const& b) {
    return [&]<size_t... I>(std::index_sequence<I...>) {
        return Vec<T, N>{ (a.data[I] * b)... };
    }(std::make_index_sequence<N>());
}

template <typename T, size_t N>
constexpr Vec<T, N> operator*(T const& a, Vec<T, N> const& b) {
    return [&]<size_t... I>(std::index_sequence<I...>) {
        return Vec<T, N>{ (a * b.data[I])... };
    }(std::make_index_sequence<N>());
}

template <typename T, size_t N>
constexpr Vec<T, N> operator/(Vec<T, N> const& a, Vec<T, N> const& b) {
    return [&]<size_t... I>(std::index_sequence<I...>) {
        return Vec<T, N>{ (a.data[I] / b.data[I])... };
    }(std::make_index_sequence<N>());
}

template <typename T, size_t N>
constexpr Vec<T, N> operator/(Vec<T, N> const& a, T const& b) {
    return [&]<size_t... I>(std::index_sequence<I...>) {
        return Vec<T, N>{ (a.data[I] / b)... };
    }(std::make_index_sequence<N>());
}

template <typename T, size_t N, typename CharT, typename TraitsT>
std::basic_ostream<CharT, TraitsT>& operator<<(
    std::basic_ostream<CharT, TraitsT>& str, Vec<T, N> const& v) {
    str << "(";
    for (bool first = true; auto& value: v) {
        str << (first ? ((void)(first = false), "") : ", ") << value;
    }
    return str << ")";
}

namespace detail {

template <typename T, size_t N>
void selector(Vec<T, N> const&);

template <typename V>
concept VecDerived = requires(V v) { detail::selector(v); };

} // namespace detail

template <detail::VecDerived V>
constexpr V min(V const& a, V const& b) {
    return [&]<size_t... I>(std::index_sequence<I...>) {
        return V{ std::min(a.data[I], b.data[I])... };
    }(std::make_index_sequence<V::size()>());
}

template <detail::VecDerived V>
constexpr V max(V const& a, V const& b) {
    return [&]<size_t... I>(std::index_sequence<I...>) {
        return V{ std::max(a.data[I], b.data[I])... };
    }(std::make_index_sequence<V::size()>());
}

template <detail::VecDerived V>
constexpr V clamp(V const& v, V const& lo, V const& hi) {
    return [&]<size_t... I>(std::index_sequence<I...>) {
        return V{ std::clamp(v.data[I], lo.data[I], hi.data[I])... };
    }(std::make_index_sequence<V::size()>());
}

template <size_t I, typename T, size_t N>
constexpr T const& get(Vec<T, N> const& v) {
    static_assert(I < N);
    return v[I];
}

template <size_t I, typename T, size_t N>
constexpr T& get(Vec<T, N>& v) {
    return const_cast<T&>(get<I>(std::as_const(v)));
}

} // namespace xui

template <typename T, size_t N>
struct std::tuple_size<xui::Vec<T, N>>: std::integral_constant<size_t, N> {};

namespace xui {

struct Point: Vec<double, 2> {
    using Vec::Vec;
    Point(Vec2<double> v): Vec(v) {}
};

template <typename T, size_t N>
struct SizeT: Vec<T, N> {
private:
    using Base = Vec<T, N>;

public:
    using Base::Base;
    constexpr SizeT(Base v): Base(v) {}

    constexpr T& width() { return this->x; }
    constexpr T const& width() const { return this->x; }
    constexpr T& height() { return this->y; }
    constexpr T const& height() const { return this->y; }
};

using Size = SizeT<double, 2>;

struct Rect: Point, Size {
    Point& origin() { return *this; };
    Point const& origin() const { return *this; };
    Size& size() { return *this; };
    Size const& size() const { return *this; };
};

constexpr Rect merge(Rect const& A, Rect const& B) {
    auto AMax = A.origin() + A.size();
    auto BMax = B.origin() + B.size();
    auto pos = min(A.origin(), B.origin());
    return { pos, max(AMax, BMax) - pos };
}

struct Color: Vec<double, 4> {
    using Vec::Vec;

    static constexpr Color White(double alpha = 1.0) {
        return { 1, 1, 1, alpha };
    }

    static constexpr Color Black(double alpha = 1.0) {
        return { 0, 0, 0, alpha };
    }

    static constexpr Color Red(double alpha = 1.0) {
        return { 1, 0, 0, alpha };
    }

    static constexpr Color Green(double alpha = 1.0) {
        return { 0, 1, 0, alpha };
    }

    static constexpr Color Blue(double alpha = 1.0) {
        return { 0, 0, 1, alpha };
    }

    static constexpr Color Yellow(double alpha = 1.0) {
        return { 1, 1, 0, alpha };
    }

    static constexpr Color Cyan(double alpha = 1.0) {
        return { 0, 1, 1, alpha };
    }

    static constexpr Color Pink(double alpha = 1.0) {
        return { 1, 0, 1, alpha };
    }

    double red() const { return data[0]; }
    double green() const { return data[1]; }
    double blue() const { return data[2]; }
    double alpha() const { return data[3]; }
};

} // namespace xui

#endif // AETHER_VEC_H
