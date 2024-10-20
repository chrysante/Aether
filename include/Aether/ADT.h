#ifndef AETHER_ADT_H
#define AETHER_ADT_H

#include <algorithm>
#include <cassert>
#include <iosfwd>
#include <memory>
#include <span>
#include <unordered_set>
#include <vector>

namespace xui {

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

/// # MoveOnlyVector

/// `std::vector` which is not constructible from a `std::initializer_list` to
/// remove ambiguity with array overloads
template <typename T>
struct MoveOnlyVector: std::vector<T> {
private:
    using Base = std::vector<T>;

public:
    MoveOnlyVector() = default;
    MoveOnlyVector(Base&& v): Base(std::move(v)) {}

    template <size_t N>
    MoveOnlyVector(T (&&elems)[N]):
        Base(std::move_iterator(std::begin(elems)),
             std::move_iterator(std::end(elems))) {}
};

template <typename T>
using UniqueVector = MoveOnlyVector<std::unique_ptr<T>>;

template <typename T>
MoveOnlyVector<T> toMoveOnlyVector(std::span<T> elems) {
    MoveOnlyVector<T> v;
    v.reserve(elems.size());
    std::move(std::begin(elems), std::end(elems), std::back_inserter(v));
    return v;
}

template <typename T, size_t N>
MoveOnlyVector<T> toMoveOnlyVector(T (&&elems)[N]) {
    return MoveOnlyVector<T>(std::move(elems));
}

namespace detail {

/// Function object that calls `.get()` on its argument and returns the result
struct GetT {
    constexpr decltype(auto) operator()(auto&& p) const {
        return std::forward<decltype(p)>(p).get();
    }
} inline constexpr Get{};

} // namespace detail

} // namespace xui

#endif // AETHER_ADT_H
