#pragma once

#include <functional>
#include <string>
#include <utility>

namespace Config {
    // unset answers false
    template <typename... Args>
    class CPredicateData {
      public:
        using FPredicate = std::function<bool(const Args&...)>;

        CPredicateData() = default;
        explicit CPredicateData(FPredicate&& fn) : m_fn(std::move(fn)) {
            ;
        }

        std::string toString() const {
            return m_fn ? "<function>" : "";
        }

        bool isSet() const {
            return !!m_fn;
        }

        bool operator()(const Args&... args) const {
            return m_fn && m_fn(args...);
        }

      private:
        FPredicate m_fn;
    };
}
