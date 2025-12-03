#pragma once

#include <cstdint>
#include <type_traits>
#include <any>
#include <ranges>
#include <algorithm>
#include "../../config/ConfigValue.hpp"

namespace Desktop::Types {

    struct SAlphaValue {
        float alpha      = 1.F;
        bool  overridden = false;

        float applyAlpha(float a) const {
            if (overridden)
                return alpha;
            else
                return alpha * a;
        };
    };

    enum eOverridePriority : uint8_t {
        PRIORITY_LAYOUT = 0,
        PRIORITY_WORKSPACE_RULE,
        PRIORITY_WINDOW_RULE,
        PRIORITY_SET_PROP,

        PRIORITY_END,
    };

    template <typename T>
    T clampOptional(T const& value, std::optional<T> const& min, std::optional<T> const& max) {
        return std::clamp(value, min.value_or(std::numeric_limits<T>::min()), max.value_or(std::numeric_limits<T>::max()));
    }

    template <typename T, bool Extended = std::is_same_v<T, bool> || std::is_same_v<T, Hyprlang::INT> || std::is_same_v<T, Hyprlang::FLOAT>>
    class COverridableVar {
      public:
        COverridableVar(T const& value, eOverridePriority priority) {
            m_values[priority] = value;
        }

        COverridableVar(T const& value) : m_defaultValue{value} {}
        COverridableVar(T const& value, std::optional<T> const& min, std::optional<T> const& max = std::nullopt) : m_defaultValue{value}, m_minValue{min}, m_maxValue{max} {}
        COverridableVar(std::string const& value)
            requires(Extended && !std::is_same_v<T, bool>)
            : m_configValue(SP<CConfigValue<T>>(new CConfigValue<T>(value))) {}
        COverridableVar(std::string const& value, std::optional<T> const& min, std::optional<T> const& max = std::nullopt)
            requires(Extended && !std::is_same_v<T, bool>)
            : m_minValue(min), m_maxValue(max), m_configValue(SP<CConfigValue<T>>(new CConfigValue<T>(value))) {}

        COverridableVar()  = default;
        ~COverridableVar() = default;

        COverridableVar& operator=(COverridableVar<T> const& other) {
            // Self-assignment check
            if (this == &other)
                return *this;

            for (size_t i = 0; i < PRIORITY_END; ++i) {
                if constexpr (Extended && !std::is_same_v<T, bool>)
                    m_values[i] = other.m_values[i].has_value() ? clampOptional(*other.m_values[i], m_minValue, m_maxValue) : other.m_values[i];
                else
                    m_values[i] = other.m_values[i];
            }

            return *this;
        }

        void set(T value, eOverridePriority priority) {
            m_values[priority] = value;
        }

        void unset(eOverridePriority priority) {
            m_values[priority] = std::nullopt;
        }

        bool hasValue() const {
            return std::ranges::any_of(m_values, [](const auto& e) { return e.has_value(); });
        }

        T value() const {
            for (const auto& v : m_values | std::ranges::views::reverse) {
                if (v)
                    return *v;
            }
            throw std::bad_optional_access();
        }

        T valueOr(T const& other) const {
            if (hasValue())
                return value();
            else
                return other;
        }

        T valueOrDefault() const
            requires(Extended && !std::is_same_v<T, bool>)
        {
            if (hasValue())
                return value();
            else if (m_defaultValue.has_value())
                return m_defaultValue.value();
            else
                return **std::any_cast<SP<CConfigValue<T>>>(m_configValue);
        }

        T valueOrDefault() const
            requires(!Extended || std::is_same_v<T, bool>)
        {
            if (hasValue())
                return value();
            else if (!m_defaultValue.has_value())
                throw std::bad_optional_access();
            else
                return m_defaultValue.value();
        }

        eOverridePriority getPriority() const {
            for (int i = PRIORITY_END - 1; i >= 0; --i) {
                if (m_values[i])
                    return sc<eOverridePriority>(i);
            }

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
        std::array<std::optional<T>, PRIORITY_END> m_values;
        std::optional<T>                           m_defaultValue; // used for toggling, so required for bool
        std::optional<T>                           m_minValue;
        std::optional<T>                           m_maxValue;
        std::any                                   m_configValue; // only there for select variables
    };

}