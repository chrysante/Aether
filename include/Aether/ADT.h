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

template <typename T>
struct Vec2 {
    Vec2() = default;
    constexpr Vec2(T value): data{ value, value } {}
    constexpr Vec2(T x, T y): data{ x, y } {}
    constexpr explicit Vec2(Axis axis, T value): Vec2() {
        (*this)[(size_t)axis] = value;
    }

    constexpr T& operator[](size_t i) {
        assert(i < 2);
        return data[i];
    }
    constexpr T const& operator[](size_t i) const {
        assert(i < 2);
        return data[i];
    }

    constexpr T* begin() { return &data[0]; }
    constexpr T const* begin() const { return &data[0]; }
    constexpr T* end() { return &data[2]; }
    constexpr T const* end() const { return &data[2]; }

    static constexpr size_t size() { return 2; }

    union {
        struct {
            T x{}, y{};
        };
        T data[2];
    };
};

template <typename T>
constexpr Vec2<T> operator+(Vec2<T> const& a, Vec2<T> const& b) {
    return { a.data[0] + b.data[0], a.data[1] + b.data[1] };
}

template <typename T>
constexpr Vec2<T> operator-(Vec2<T> const& a, Vec2<T> const& b) {
    return { a.data[0] - b.data[0], a.data[1] - b.data[1] };
}

template <typename T>
constexpr Vec2<T> operator*(Vec2<T> const& a, Vec2<T> const& b) {
    return { a.data[0] * b.data[0], a.data[1] * b.data[1] };
}

template <typename T>
constexpr Vec2<T> operator/(Vec2<T> const& a, Vec2<T> const& b) {
    return { a.data[0] / b.data[0], a.data[1] / b.data[1] };
}

namespace detail {

template <typename T>
void selector(Vec2<T> const&);

template <typename V>
concept Vec2Derived = requires(V v) { detail::selector(v); };

} // namespace detail

template <detail::Vec2Derived V>
constexpr V min(V const& a, V const& b) {
    return { std::min(a.x, b.x), std::min(a.y, b.y) };
}

template <detail::Vec2Derived V>
constexpr V max(V const& a, V const& b) {
    return { std::max(a.x, b.x), std::max(a.y, b.y) };
}

template <detail::Vec2Derived V>
constexpr V clamp(V const& v, V const& lo, V const& hi) {
    return { std::clamp(v.x, lo.x, hi.x), std::clamp(v.y, lo.y, hi.y) };
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

struct Position: Vec2<double> {
    using Vec2::Vec2;
};

struct Size: Vec2<double> {
    using Vec2::Vec2;
    constexpr Size(Vec2<double> v): Vec2(v) {}

    constexpr double& width() { return x; }
    constexpr double width() const { return x; }
    constexpr double& height() { return y; }
    constexpr double height() const { return y; }
};

struct Rect {
    Position pos;
    Size size;
};

constexpr Rect merge(Rect const& A, Rect const& B) {
    auto AMax = A.pos + A.size;
    auto BMax = B.pos + B.size;
    auto pos = min(A.pos, B.pos);
    return { pos, max(AMax, BMax) - pos };
}

struct Color {
    Color(double r, double g, double b, double a): data{ r, g, b, a } {}

    double red() const { return data[0]; }
    double green() const { return data[1]; }
    double blue() const { return data[2]; }
    double alpha() const { return data[3]; }

private:
    double data[4];
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
        return *this;
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

    T& assign(T& ptr) { return *assign(&ptr); }

    T* assign(T* ptr) {
        this->_assign(ptr);
        return ptr;
    }

    std::unique_ptr<T> assign(std::unique_ptr<T>&& ptr) {
        this->_assign(ptr.get());
        return std::move(ptr);
    }

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
