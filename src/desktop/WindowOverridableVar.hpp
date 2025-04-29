#pragma once

#include <cstdint>
#include <type_traits>
#include <any>
#include "../config/ConfigValue.hpp"

enum eOverridePriority : uint8_t {
    PRIORITY_LAYOUT = 0,
    PRIORITY_WORKSPACE_RULE,
    PRIORITY_WINDOW_RULE,
    PRIORITY_SET_PROP,
};

template <typename T>
T clampOptional(T const& value, std::optional<T> const& min, std::optional<T> const& max) {
    return std::clamp(value, min.value_or(std::numeric_limits<T>::min()), max.value_or(std::numeric_limits<T>::max()));
}

template <typename T, bool Extended = std::is_same_v<T, bool> || std::is_same_v<T, Hyprlang::INT> || std::is_same_v<T, Hyprlang::FLOAT>>
class CWindowOverridableVar {
  public:
    CWindowOverridableVar(T const& value, eOverridePriority priority) {
        m_values[priority] = value;
    }

    CWindowOverridableVar(T const& value) : m_defaultValue{value} {}
    CWindowOverridableVar(T const& value, std::optional<T> const& min, std::optional<T> const& max = std::nullopt) : m_defaultValue{value}, m_minValue{min}, m_maxValue{max} {}
    CWindowOverridableVar(std::string const& value)
        requires(Extended && !std::is_same_v<T, bool>)
        : m_configValue(SP<CConfigValue<T>>(new CConfigValue<T>(value))) {}
    CWindowOverridableVar(std::string const& value, std::optional<T> const& min, std::optional<T> const& max = std::nullopt)
        requires(Extended && !std::is_same_v<T, bool>)
        : m_minValue(min), m_maxValue(max), m_configValue(SP<CConfigValue<T>>(new CConfigValue<T>(value))) {}

    CWindowOverridableVar()  = default;
    ~CWindowOverridableVar() = default;

    CWindowOverridableVar& operator=(CWindowOverridableVar<T> const& other) {
        // Self-assignment check
        if (this == &other)
            return *this;

        for (auto const& value : other.m_values) {
            if constexpr (Extended && !std::is_same_v<T, bool>)
                m_values[value.first] = clampOptional(value.second, m_minValue, m_maxValue);
            else
                m_values[value.first] = value.second;
        }

        return *this;
    }

    void unset(eOverridePriority priority) {
        m_values.erase(priority);
    }

    bool hasValue() {
        return !m_values.empty();
    }

    T value() {
        if (!m_values.empty())
            return std::prev(m_values.end())->second;
        else
            throw std::bad_optional_access();
    }

    T valueOr(T const& other) {
        if (hasValue())
            return value();
        else
            return other;
    }

    T valueOrDefault()
        requires(Extended && !std::is_same_v<T, bool>)
    {
        if (hasValue())
            return value();
        else if (m_defaultValue.has_value())
            return m_defaultValue.value();
        else
            return **std::any_cast<SP<CConfigValue<T>>>(m_configValue);
    }

    T valueOrDefault()
        requires(!Extended || std::is_same_v<T, bool>)
    {
        if (hasValue())
            return value();
        else if (!m_defaultValue.has_value())
            throw std::bad_optional_access();
        else
            return m_defaultValue.value();
    }

    eOverridePriority getPriority() {
        if (!m_values.empty())
            return std::prev(m_values.end())->first;
        else
            throw std::bad_optional_access();
    }

    void increment(T const& other, eOverridePriority priority) {
        if constexpr (std::is_same_v<T, bool>)
            m_values[priority] = valueOr(false) ^ other;
        else
            m_values[priority] = clampOptional(valueOrDefault() + other, m_minValue, m_maxValue);
    }

    void matchOptional(std::optional<T> const& optValue, eOverridePriority priority) {
        if (optValue.has_value())
            m_values[priority] = optValue.value();
        else
            unset(priority);
    }

    operator std::optional<T>() {
        if (hasValue())
            return value();
        else
            return std::nullopt;
    }

  private:
    std::map<eOverridePriority, T> m_values;
    std::optional<T>               m_defaultValue; // used for toggling, so required for bool
    std::optional<T>               m_minValue;
    std::optional<T>               m_maxValue;
    std::any                       m_configValue; // only there for select variables
};
