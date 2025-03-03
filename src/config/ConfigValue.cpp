#pragma once
#include "ConfigValue.hpp"

template <typename T>
class CConfigValue {
  public:
    CConfigValue(const std::string& val) {
        const auto PVHYPRLANG = g_pConfigManager->getHyprlangConfigValuePtr(val);
    };

    T* ptr() const {
        return *(T* const*)m_p_;
    }

    T operator*() const {
        return *ptr();
    }
};

template <>
inline std::string* CConfigValue<std::string>::ptr() const {
    RASSERT(false, "Impossible to implement ptr() of CConfigValue<std::string>");
    return nullptr;
}

template <>
inline std::string CConfigValue<std::string>::operator*() const {
    return std::string{*(Hyprlang::STRING*)m_p_};
}

template <>
inline Hyprlang::STRING* CConfigValue<Hyprlang::STRING>::ptr() const {
    return (Hyprlang::STRING*)m_p_;
}

template <>
inline Hyprlang::STRING CConfigValue<Hyprlang::STRING>::operator*() const {
    return *(Hyprlang::STRING*)m_p_;
}

template <>
inline Hyprlang::CUSTOMTYPE* CConfigValue<Hyprlang::CUSTOMTYPE>::ptr() const {
    return *(Hyprlang::CUSTOMTYPE* const*)m_p_;
}

template <>
inline Hyprlang::CUSTOMTYPE CConfigValue<Hyprlang::CUSTOMTYPE>::operator*() const {
    RASSERT(false, "Impossible to implement operator* of CConfigValue<Hyprlang::CUSTOMTYPE>, use ptr()");
    return *ptr();
}
