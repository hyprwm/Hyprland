#pragma once

#include <functional>
#include <string>
#include "../memory/Memory.hpp"

// TODO: move into hyprutils

template <typename T>
class CPromise;
template <typename T>
class CPromiseResult;

template <typename T>
class CPromiseResolver {
  public:
    CPromiseResolver(const CPromiseResolver&)            = delete;
    CPromiseResolver(CPromiseResolver&&)                 = delete;
    CPromiseResolver& operator=(const CPromiseResolver&) = delete;
    CPromiseResolver& operator=(CPromiseResolver&&)      = delete;

    void              resolve(T value) {
        if (m_promise->m_result)
            return;

        m_promise->m_result = CPromiseResult<T>::result(value);

        if (!m_promise->m_then)
            return;

        m_promise->m_then(m_promise->m_result);
    }

    void reject(const std::string& reason) {
        if (m_promise->m_result)
            return;

        m_promise->m_result = CPromiseResult<T>::err(reason);

        if (!m_promise->m_then)
            return;

        m_promise->m_then(m_promise->m_result);
    }

  private:
    CPromiseResolver(SP<CPromise<T>> promise) : m_promise(promise) {}

    SP<CPromise<T>> m_promise;

    friend class CPromise<T>;
};

template <typename T>
class CPromiseResult {
  public:
    bool hasError() {
        return m_hasError;
    }

    T result() {
        return m_result;
    }

    std::string error() {
        return m_error;
    }

  private:
    static SP<CPromiseResult<T>> result(T result) {
        auto p      = SP<CPromiseResult<T>>(new CPromiseResult<T>());
        p->m_result = result;
        return p;
    }

    static SP<CPromiseResult<T>> err(std::string reason) {
        auto p        = SP<CPromiseResult<T>>(new CPromiseResult<T>());
        p->m_error    = reason;
        p->m_hasError = true;
        return p;
    }

    T           m_result   = {};
    std::string m_error    = {};
    bool        m_hasError = false;

    friend class CPromiseResolver<T>;
};

template <typename T>
class CPromise {
  public:
    CPromise(const CPromise&)                      = delete;
    CPromise(CPromise&&)                           = delete;
    CPromise&           operator=(const CPromise&) = delete;
    CPromise&           operator=(CPromise&&)      = delete;

    static SP<CPromise> make(const std::function<void(SP<CPromiseResolver<T>>)>& fn) {
        auto sp = SP<CPromise<T>>(new CPromise<T>());
        fn(SP<CPromiseResolver<T>>(new CPromiseResolver<T>(sp)));
        return sp;
    }

    void then(std::function<void(SP<CPromiseResult<T>>)>&& fn) {
        m_then = std::move(fn);
        if (m_result)
            m_then(m_result);
    }

  private:
    CPromise() = default;

    const std::function<void(SP<CPromiseResult<T>>)> m_fn;
    std::function<void(SP<CPromiseResult<T>>)>       m_then;
    SP<CPromiseResult<T>>                            m_result;

    friend class CPromiseResult<T>;
    friend class CPromiseResolver<T>;
};