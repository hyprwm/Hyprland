#pragma once

#include <string>
#include <typeindex>
#include <hyprlang.hpp>
#include "../debug/Log.hpp"
#include "../macros.hpp"
#include "ConfigManager.hpp"

template <typename T>
class CConfigValue {
  public:
    CConfigValue(const std::string& val) {
        const auto PVHYPRLANG = g_pConfigManager->getHyprlangConfigValuePtr(val);

        // NOLINTNEXTLINE
        p_ = PVHYPRLANG->getDataStaticPtr();

#ifdef HYPRLAND_DEBUG
        // verify type
        const auto ANY  = PVHYPRLANG->getValue();
        const auto TYPE = std::type_index(ANY.type());

        // exceptions
        const bool STRINGEX = (typeid(T) == typeid(std::string) && TYPE == typeid(Hyprlang::STRING));
        const bool CUSTOMEX = (typeid(T) == typeid(Hyprlang::CUSTOMTYPE) && (TYPE == typeid(Hyprlang::CUSTOMTYPE*) || TYPE == typeid(void*) /* dunno why it does this? */));

        RASSERT(typeid(T) == TYPE || STRINGEX || CUSTOMEX, "Mismatched type in CConfigValue<T>, got {} but has {}", typeid(T).name(), TYPE.name());
#endif
    }

    T* ptr() const {
        return *(T* const*)p_;
    }

    T operator*() const {
        return *ptr();
    }

  private:
    void* const* p_ = nullptr;
};

template <>
inline std::string* CConfigValue<std::string>::ptr() const {
    RASSERT(false, "Impossible to implement ptr() of CConfigValue<std::string>");
    return nullptr;
}

template <>
inline std::string CConfigValue<std::string>::operator*() const {
    return std::string{*(Hyprlang::STRING*)p_};
}

template <>
inline Hyprlang::STRING* CConfigValue<Hyprlang::STRING>::ptr() const {
    return (Hyprlang::STRING*)p_;
}

template <>
inline Hyprlang::STRING CConfigValue<Hyprlang::STRING>::operator*() const {
    return *(Hyprlang::STRING*)p_;
}

template <>
inline Hyprlang::CUSTOMTYPE* CConfigValue<Hyprlang::CUSTOMTYPE>::ptr() const {
    return *(Hyprlang::CUSTOMTYPE* const*)p_;
}

template <>
inline Hyprlang::CUSTOMTYPE CConfigValue<Hyprlang::CUSTOMTYPE>::operator*() const {
    RASSERT(false, "Impossible to implement operator* of CConfigValue<Hyprlang::CUSTOMTYPE>, use ptr()");
    return *ptr();
}