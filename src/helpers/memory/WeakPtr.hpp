#pragma once

#include "SharedPtr.hpp"

#define WP CWeakPointer

/*
    This is a Hyprland implementation of std::weak_ptr.

    See SharedPtr.hpp for more info on how it's different.
*/

template <typename T>
class CWeakPointer {
  public:
    template <typename X>
    using validHierarchy = typename std::enable_if<std::is_assignable<CWeakPointer<T>&, X>::value, CWeakPointer&>::type;
    template <typename X>
    using isConstructible = typename std::enable_if<std::is_constructible<T&, X&>::value>::type;

    /* create a weak ptr from a reference */
    template <typename U, typename = isConstructible<U>>
    CWeakPointer(const CSharedPointer<U>& ref) noexcept {
        if (!ref.impl_)
            return;

        impl_ = ref.impl_;
        incrementWeak();
    }

    /* create a weak ptr from another weak ptr */
    template <typename U, typename = isConstructible<U>>
    CWeakPointer(const CWeakPointer<U>& ref) noexcept {
        if (!ref.impl_)
            return;

        impl_ = ref.impl_;
        incrementWeak();
    }

    CWeakPointer(const CWeakPointer& ref) noexcept {
        if (!ref.impl_)
            return;

        impl_ = ref.impl_;
        incrementWeak();
    }

    template <typename U, typename = isConstructible<U>>
    CWeakPointer(CWeakPointer<U>&& ref) noexcept {
        std::swap(impl_, ref.impl_);
    }

    CWeakPointer(CWeakPointer&& ref) noexcept {
        std::swap(impl_, ref.impl_);
    }

    /* create a weak ptr from another weak ptr with assignment */
    template <typename U>
    validHierarchy<const CWeakPointer<U>&> operator=(const CWeakPointer<U>& rhs) {
        if (impl_ == rhs.impl_)
            return *this;

        decrementWeak();
        impl_ = rhs.impl_;
        incrementWeak();
        return *this;
    }

    CWeakPointer<T>& operator=(const CWeakPointer& rhs) {
        if (impl_ == rhs.impl_)
            return *this;

        decrementWeak();
        impl_ = rhs.impl_;
        incrementWeak();
        return *this;
    }

    /* create a weak ptr from a shared ptr with assignment */
    template <typename U>
    validHierarchy<const CWeakPointer<U>&> operator=(const CSharedPointer<U>& rhs) {
        if ((uintptr_t)impl_ == (uintptr_t)rhs.impl_)
            return *this;

        decrementWeak();
        impl_ = rhs.impl_;
        incrementWeak();
        return *this;
    }

    /* create an empty weak ptr */
    CWeakPointer() {
        ;
    }

    ~CWeakPointer() {
        decrementWeak();
    }

    /* expired MAY return true even if the pointer is still stored.
       the situation would be e.g. self-weak pointer in a destructor.
       for pointer validity, use valid() */
    bool expired() const {
        return !impl_ || !impl_->dataNonNull() || impl_->destroying();
    }

    /* this means the pointed-to object is not yet deleted and can still be
       referenced, but it might be in the process of being deleted. 
       check !expired() if you want to check whether it's valid and
       assignable to a SP. */
    bool valid() const {
        return impl_ && impl_->dataNonNull();
    }

    void reset() {
        decrementWeak();
        impl_ = nullptr;
    }

    CSharedPointer<T> lock() const {
        if (!impl_ || !impl_->dataNonNull() || impl_->destroying())
            return {};

        return CSharedPointer<T>(impl_);
    }

    /* this returns valid() */
    operator bool() const {
        return valid();
    }

    bool operator==(const CWeakPointer<T>& rhs) const {
        return impl_ == rhs.impl_;
    }

    bool operator==(const CSharedPointer<T>& rhs) const {
        return impl_ == rhs.impl_;
    }

    bool operator()(const CWeakPointer& lhs, const CWeakPointer& rhs) const {
        return (uintptr_t)lhs.impl_ < (uintptr_t)rhs.impl_;
    }

    bool operator<(const CWeakPointer& rhs) const {
        return (uintptr_t)impl_ < (uintptr_t)rhs.impl_;
    }

    T* get() const {
        return (T*)(impl_ ? static_cast<CSharedPointer_::impl<T>*>(impl_)->_data : nullptr);
    }

    T* operator->() const {
        return get();
    }

    CSharedPointer_::impl_base* impl_ = nullptr;

  private:
    /* no-op if there is no impl_ */
    void decrementWeak() {
        if (!impl_)
            return;

        impl_->decWeak();

        // we need to check for ->destroying,
        // because otherwise we could destroy here
        // and have a shared_ptr destroy the same thing
        // later (in situations where we have a weak_ptr to self)
        if (impl_->wref() == 0 && impl_->ref() == 0 && !impl_->destroying()) {
            delete impl_;
            impl_ = nullptr;
        }
    }
    /* no-op if there is no impl_ */
    void incrementWeak() {
        if (!impl_)
            return;

        impl_->incWeak();
    }
};

template <typename T>
struct std::hash<CWeakPointer<T>> {
    std::size_t operator()(const CWeakPointer<T>& p) const noexcept {
        return std::hash<void*>{}(p.impl_);
    }
};
