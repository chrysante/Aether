#ifndef AETHER_ADT_H
#define AETHER_ADT_H

#include <algorithm>
#include <cassert>
#include <memory>
#include <span>
#include <unordered_set>
#include <vector>

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
    constexpr VecData(T value): data{ value, value } {}

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
    constexpr VecData(T value): data{ value, value, value } {}

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
    constexpr VecData(T value): data{ value, value, value, value } {}

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

    Vec() = default;

    using Base::Base;

    constexpr explicit Vec(Axis axis, T value): Vec() {
        (*this)[(size_t)axis] = value;
    }

    constexpr T& operator[](size_t i) {
        assert(i < N);
        return data[i];
    }
    constexpr T const& operator[](size_t i) const {
        assert(i < N);
        return data[i];
    }

    constexpr T* begin() { return &data[0]; }
    constexpr T const* begin() const { return &data[0]; }
    constexpr T* end() { return &data[N]; }
    constexpr T const* end() const { return &data[N]; }

    static constexpr size_t size() { return N; }
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
constexpr Vec<T, N> operator/(Vec<T, N> const& a, Vec<T, N> const& b) {
    return [&]<size_t... I>(std::index_sequence<I...>) {
        return Vec<T, N>{ (a.data[I] / b.data[I])... };
    }(std::make_index_sequence<N>());
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

template <size_t I, typename T>
constexpr T const& get(Vec2<T> const& v) {
    static_assert(I < 2);
    if constexpr (I == 0) return v.x;
    if constexpr (I == 1) return v.y;
}

template <size_t I, typename T>
constexpr T& get(Vec2<T>& v) {
    return const_cast<T&>(get<I>(std::as_const(v)));
}

struct Position: Vec<double, 2> {
    using Vec::Vec;
};

struct Size: Vec<double, 2> {
    using Vec::Vec;
    constexpr Size(Vec2<double> v): Vec(v) {}

    constexpr double& width() { return x; }
    constexpr double width() const { return x; }
    constexpr double& height() { return y; }
    constexpr double height() const { return y; }
};

struct Rect: Position, Size {
    Position& pos() { return *this; };
    Position const& pos() const { return *this; };
    Size& size() { return *this; };
    Size const& size() const { return *this; };
};

constexpr Rect merge(Rect const& A, Rect const& B) {
    auto AMax = A.pos() + A.size();
    auto BMax = B.pos() + B.size();
    auto pos = min(A.pos(), B.pos());
    return { pos, max(AMax, BMax) - pos };
}

struct Color: Vec<double, 4> {
    using Vec::Vec;

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

enum class LayoutMode { Static, Flex };

/// # WeakRef

template <typename>
class WeakRef;

namespace detail {

template <typename>
class WeakRefImpl;

struct WeakRefCountableTag {};

} // namespace detail

template <typename Derived>
class WeakRefCountableBase: public detail::WeakRefCountableTag {
protected:
    WeakRefCountableBase() = default;
    WeakRefCountableBase(WeakRefCountableBase const&) = delete;
    WeakRefCountableBase& operator=(WeakRefCountableBase const&) = delete;
    ~WeakRefCountableBase() {
        for (auto& ref: _refs) {
            ref->_ptr = nullptr;
        }
    }

private:
    template <typename>
    friend class WeakRef;
    template <typename>
    friend class detail::WeakRefImpl;

    using WeakRefBaseType = Derived;

    void addRef(detail::WeakRefImpl<Derived>* ref) { _refs.insert(ref); }

    void eraseRef(detail::WeakRefImpl<Derived> const* ref) {
        _refs.erase(const_cast<detail::WeakRefImpl<Derived>*>(ref));
    }

    std::unordered_set<detail::WeakRefImpl<Derived>*> _refs;
};

template <typename T>
concept WeakRefCountable = std::derived_from<T, detail::WeakRefCountableTag>;

template <typename T>
class detail::WeakRefImpl {
public:
    WeakRefImpl() = default;

    WeakRefImpl(WeakRefImpl const& other): WeakRefImpl(other._ptr) {}

    WeakRefImpl& operator=(WeakRefImpl const& other) {
        _assign(other._ptr);
        return *this;
    }

    ~WeakRefImpl() { release(); }

protected:
    WeakRefImpl(T* arg): _ptr(arg) { retain(); }

    void _assign(T* other) {
        release();
        _ptr = other;
        retain();
    }

private:
    template <typename>
    friend class xui::WeakRef;
    friend class WeakRefCountableBase<T>;

    void retain() {
        if (_ptr) {
            _ptr->addRef(this);
        }
    }
    void release() {
        if (_ptr) {
            _ptr->eraseRef(this);
        }
    }

    T* _ptr = nullptr;
};

template <WeakRefCountable T>
class WeakRef<T>: detail::WeakRefImpl<typename T::WeakRefBaseType> {
    using Impl = detail::WeakRefImpl<typename T::WeakRefBaseType>;

public:
    WeakRef() = default;

    WeakRef(T& arg): Impl(&arg) {}

    WeakRef(T* arg): Impl(arg) {}

    WeakRef(std::unique_ptr<T> const& arg): Impl(arg.get()) {}

    WeakRef& operator=(T& arg) { this->_assign(&arg); }

    T* get() const { return static_cast<T*>(this->_ptr); }

    T* operator->() const { return get(); }

    T& operator*() const { return *get(); }

    explicit operator bool() const { return get() != nullptr; }
};

template <typename T>
WeakRef(T) -> WeakRef<T>;

template <typename T>
WeakRef(std::unique_ptr<T>) -> WeakRef<T>;

/// # UniqueVector

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
