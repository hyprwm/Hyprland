#pragma once

#include "ConfigValue.hpp"

#include <functional>
#include <utility>

template <typename T>
class CRedirectableConfigValue {
  public:
    CRedirectableConfigValue() = default;
    explicit CRedirectableConfigValue(const std::string& val, const std::string& fallback, std::function<T(T)>&& transformer = nullptr) :
        m_target(val), m_fallback(fallback), m_transformer(std::move(transformer)) {
        ;
    }

    T operator*() const {
        if (m_target.setByUser())
            return *m_target;

        return m_transformer ? m_transformer(*m_fallback) : *m_fallback;
    }

    bool good() const {
        return m_target.good() && m_fallback.good();
    }

  private:
    CConfigValue<T>     m_target, m_fallback;
    std::function<T(T)> m_transformer = nullptr;
};
