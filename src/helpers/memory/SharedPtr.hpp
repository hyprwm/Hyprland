#pragma once

#include <typeinfo>
#include <typeindex>
#include <cstddef>
#include <cstdint>
#include <memory>

#define SP CSharedPointer

/*
    This is a custom impl of std::shared_ptr.
    It is not thread-safe like the STL one,
    but Hyprland is single-threaded anyways.

    It differs a bit from how the STL one works,
    namely in the fact that it keeps the T* inside the
    control block, and that you can still make a CWeakPtr
    or deref an existing one inside the destructor.
*/
namespace CSharedPointer_ {

    class impl_base {
      public:
        virtual ~impl_base(){};

        virtual void         inc() noexcept         = 0;
        virtual void         dec() noexcept         = 0;
        virtual void         incWeak() noexcept     = 0;
        virtual void         decWeak() noexcept     = 0;
        virtual unsigned int ref() noexcept         = 0;
        virtual unsigned int wref() noexcept        = 0;
        virtual void         destroy() noexcept     = 0;
        virtual bool         destroying() noexcept  = 0;
        virtual bool         dataNonNull() noexcept = 0;
    };

    template <typename T>
    class impl : public impl_base {
      public:
        impl(T* data) noexcept : _data(data) {
            ;
        }

        /* strong refcount */
        unsigned int _ref = 0;
        /* weak refcount */
        unsigned int _weak = 0;

        T*           _data = nullptr;

        friend void  swap(impl*& a, impl*& b) {
            impl* tmp = a;
            a         = b;
            b         = tmp;
        }

        /* if the destructor was called, 
           creating shared_ptrs is no longer valid */
        bool _destroying = false;

        void _destroy() {
            if (!_data)
                return;

            // first, we destroy the data, but keep the pointer.
            // this way, weak pointers will still be able to
            // reference and use, but no longer create shared ones.
            _destroying = true;
            __deleter(_data);
            // now, we can reset the data and call it a day.
            _data       = nullptr;
            _destroying = false;
        }

        std::default_delete<T> __deleter{};

        //
        virtual void inc() noexcept {
            _ref++;
        }

        virtual void dec() noexcept {
            _ref--;
        }

        virtual void incWeak() noexcept {
            _weak++;
        }

        virtual void decWeak() noexcept {
            _weak--;
        }

        virtual unsigned int ref() noexcept {
            return _ref;
        }

        virtual unsigned int wref() noexcept {
            return _weak;
        }

        virtual void destroy() noexcept {
            _destroy();
        }

        virtual bool destroying() noexcept {
            return _destroying;
        }

        virtual bool dataNonNull() noexcept {
            return _data;
        }

        virtual ~impl() {
            destroy();
        }
    };
};

template <typename T>
class CSharedPointer {
  public:
    template <typename X>
    using validHierarchy = typename std::enable_if<std::is_assignable<CSharedPointer<T>&, X>::value, CSharedPointer&>::type;
    template <typename X>
    using isConstructible = typename std::enable_if<std::is_constructible<T&, X&>::value>::type;

    /* creates a new shared pointer managing a resource
       avoid calling. Could duplicate ownership. Prefer makeShared */
    explicit CSharedPointer(T* object) noexcept {
        impl_ = new CSharedPointer_::impl<T>(object);
        increment();
    }

    /* creates a shared pointer from a reference */
    template <typename U, typename = isConstructible<U>>
    CSharedPointer(const CSharedPointer<U>& ref) noexcept {
        impl_ = ref.impl_;
        increment();
    }

    CSharedPointer(const CSharedPointer& ref) noexcept {
        impl_ = ref.impl_;
        increment();
    }

    template <typename U, typename = isConstructible<U>>
    CSharedPointer(CSharedPointer<U>&& ref) noexcept {
        std::swap(impl_, ref.impl_);
    }

    CSharedPointer(CSharedPointer&& ref) noexcept {
        std::swap(impl_, ref.impl_);
    }

    /* allows weakPointer to create from an impl */
    CSharedPointer(CSharedPointer_::impl_base* implementation) noexcept {
        impl_ = implementation;
        increment();
    }

    /* creates an empty shared pointer with no implementation */
    CSharedPointer() noexcept {
        ; // empty
    }

    /* creates an empty shared pointer with no implementation */
    CSharedPointer(std::nullptr_t) noexcept {
        ; // empty
    }

    ~CSharedPointer() {
        // we do not decrement here,
        // because we want to preserve the pointer
        // in case this is the last owner.
        if (impl_ && impl_->ref() == 1)
            destroyImpl();
        else
            decrement();
    }

    template <typename U>
    validHierarchy<const CSharedPointer<U>&> operator=(const CSharedPointer<U>& rhs) {
        if (impl_ == rhs.impl_)
            return *this;

        decrement();
        impl_ = rhs.impl_;
        increment();
        return *this;
    }

    CSharedPointer& operator=(const CSharedPointer& rhs) {
        if (impl_ == rhs.impl_)
            return *this;

        decrement();
        impl_ = rhs.impl_;
        increment();
        return *this;
    }

    template <typename U>
    validHierarchy<const CSharedPointer<U>&> operator=(CSharedPointer<U>&& rhs) {
        std::swap(impl_, rhs.impl_);
        return *this;
    }

    CSharedPointer& operator=(CSharedPointer&& rhs) {
        std::swap(impl_, rhs.impl_);
        return *this;
    }

    operator bool() const {
        return impl_ && impl_->dataNonNull();
    }

    bool operator==(const CSharedPointer& rhs) const {
        return impl_ == rhs.impl_;
    }

    bool operator()(const CSharedPointer& lhs, const CSharedPointer& rhs) const {
        return (uintptr_t)lhs.impl_ < (uintptr_t)rhs.impl_;
    }

    bool operator<(const CSharedPointer& rhs) const {
        return (uintptr_t)impl_ < (uintptr_t)rhs.impl_;
    }

    T* operator->() const {
        return get();
    }

    T& operator*() const {
        return *get();
    }

    void reset() {
        decrement();
        impl_ = nullptr;
    }

    T* get() const {
        return (T*)(impl_ ? static_cast<CSharedPointer_::impl<T>*>(impl_)->_data : nullptr);
    }

    unsigned int strongRef() const {
        return impl_ ? impl_->ref() : 0;
    }

    CSharedPointer_::impl_base* impl_ = nullptr;

  private:
    /* 
       no-op if there is no impl_
       may delete the stored object if ref == 0
       may delete and reset impl_ if ref == 0 and weak == 0
    */
    void decrement() {
        if (!impl_)
            return;

        impl_->dec();

        // if ref == 0, we can destroy impl
        if (impl_->ref() == 0)
            destroyImpl();
    }
    /* no-op if there is no impl_ */
    void increment() {
        if (!impl_)
            return;

        impl_->inc();
    }

    /* destroy the pointed-to object 
       if able, will also destroy impl */
    void destroyImpl() {
        // destroy the impl contents
        impl_->destroy();

        // check for weak refs, if zero, we can also delete impl_
        if (impl_->wref() == 0) {
            delete impl_;
            impl_ = nullptr;
        }
    }
};

template <typename U, typename... Args>
static CSharedPointer<U> makeShared(Args&&... args) {
    return CSharedPointer<U>(new U(std::forward<Args>(args)...));
}

template <typename T>
struct std::hash<CSharedPointer<T>> {
    std::size_t operator()(const CSharedPointer<T>& p) const noexcept {
        return std::hash<void*>{}(p->impl_);
    }
};
