#pragma once
#include <string>
#include <typeindex>
#include <hyprlang.hpp>
#include "../macros.hpp"
#include "ConfigManager.hpp"

template <typename T>
class CConfigValue {
  public:
    CConfigValue(const std::string& val);

    // NOLINTNEXTLINE
    m_p_ = PVHYPRLANG->getDataStaticPtr();

#ifdef HYPRLAND_DEBUG
    // verify type
    const auto m_ANY  = PVHYPRLANG->getValue();
    const auto m_TYPE = std::type_index(ANY.type());

    // exceptions
    const bool m_STRINGEX = (typeid(T) == typeid(std::string) && m_TYPE == typeid(Hyprlang::STRING));
    const bool m_CUSTOMEX = (typeid(T) == typeid(Hyprlang::CUSTOMTYPE) && (m_TYPE == typeid(Hyprlang::CUSTOMTYPE*) || m_TYPE == typeid(void*) /* dunno why it does this? */));

    RASSERT(typeid(T) == m_TYPE || m_STRINGEX || m_CUSTOMEX, "Mismatched type in CConfigValue<T>, got {} but has {}", typeid(T).name(), m_TYPE.name());
#endif

    T* ptr() const;

    T  operator*() const;

  private:
    void* const* m_p_ = nullptr;
};

template <>
std::string* CConfigValue<std::string>::ptr() const;

template <>
std::string CConfigValue<std::string>::operator*() const;

template <>
Hyprlang::STRING* CConfigValue<Hyprlang::STRING>::ptr() const;

template <>
Hyprlang::STRING CConfigValue<Hyprlang::STRING>::operator*() const;

template <>
Hyprlang::CUSTOMTYPE* CConfigValue<Hyprlang::CUSTOMTYPE>::ptr() const;

template <>
Hyprlang::CUSTOMTYPE CConfigValue<Hyprlang::CUSTOMTYPE>::operator*() const;
