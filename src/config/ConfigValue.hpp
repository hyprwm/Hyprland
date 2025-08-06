#pragma once

#include <string>
#include <typeindex>
#include <hyprlang.hpp>
#include "../macros.hpp"

// giga hack to avoid including configManager here
// NOLINTNEXTLINE
void            local__configValuePopulate(void* const** p, const std::string& val);
std::type_index local__configValueTypeIdx(const std::string& val);

template <typename T>
class CConfigValue {
  public:
    CConfigValue(const std::string& val) {
#ifdef HYPRLAND_DEBUG
        // verify type
        const auto TYPE = local__configValueTypeIdx(val);

        // exceptions
        const bool STRINGEX = (typeid(T) == typeid(std::string) && TYPE == typeid(Hyprlang::STRING));
        const bool CUSTOMEX = (typeid(T) == typeid(Hyprlang::CUSTOMTYPE) && (TYPE == typeid(Hyprlang::CUSTOMTYPE*) || TYPE == typeid(void*) /* dunno why it does this? */));

        RASSERT(typeid(T) == TYPE || STRINGEX || CUSTOMEX, "Mismatched type in CConfigValue<T>, got {} but has {}", typeid(T).name(), TYPE.name());
#endif

        local__configValuePopulate(&p_, val);
    }

    T* ptr() const {
        return *static_cast<T* const*>(p_);
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
    return *reinterpret_cast<Hyprlang::CUSTOMTYPE* const*>(p_);
}

template <>
inline Hyprlang::CUSTOMTYPE CConfigValue<Hyprlang::CUSTOMTYPE>::operator*() const {
    RASSERT(false, "Impossible to implement operator* of CConfigValue<Hyprlang::CUSTOMTYPE>, use ptr()");
    return *ptr();
}
