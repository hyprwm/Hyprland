#pragma once

#include <string>
#include <typeindex>
#include <hyprlang.hpp>
#include "../macros.hpp"
#include "ConfigManager.hpp"

/**
 * @class CConfigValue
 * @brief A template class to handle configuration values.
 *
 * This class provides a type-safe way to access configuration values.
 * It supports custom types defined in Hyprlang.
 *
 * @tparam T Type of the configuration value.
 */
template <typename T>
class CConfigValue {
  public:
    /**
     * @brief Constructor that initializes the configuration value.
     * @param val The key to look up the configuration value.
     */
    CConfigValue(const std::string& val) {
        const auto PVHYPRLANG = g_pConfigManager->getHyprlangConfigValuePtr(val);

        p_ = PVHYPRLANG->getDataStaticPtr();

#ifdef HYPRLAND_DEBUG
        // Verify type during debug mode
        const auto ANY  = PVHYPRLANG->getValue();
        const auto TYPE = std::type_index(ANY.type());

        // Handle specific type exceptions
        const bool STRINGEX = (typeid(T) == typeid(std::string) && TYPE == typeid(Hyprlang::STRING));
        const bool CUSTOMEX = (typeid(T) == typeid(Hyprlang::CUSTOMTYPE) && (TYPE == typeid(Hyprlang::CUSTOMTYPE*) || TYPE == typeid(void*) /* dunno why it does this? */));

        RASSERT(typeid(T) == TYPE || STRINGEX || CUSTOMEX, "Mismatched type in CConfigValue<T>, got {} but has {}", typeid(T).name(), TYPE.name());
#endif
    }

    /**
     * @brief Returns a pointer to the configuration value.
     * @return Pointer to the configuration value.
     */
    T* ptr() const {
        return *(T* const*)p_;
    }

    /**
     * @brief Dereference operator to access the configuration value.
     * @return The configuration value.
     */
    T operator*() const {
        return *ptr();
    }

  private:
    void* const* p_ = nullptr; //< Pointer to the configuration value.
};

// Specializations for std::string

/**
 * @brief Specialization of ptr() for std::string.
 * @return nullptr since this implementation is not supported.
 */
template <>
inline std::string* CConfigValue<std::string>::ptr() const {
    RASSERT(false, "Impossible to implement ptr() of CConfigValue<std::string>");
    return nullptr;
}

/**
 * @brief Specialization of operator* for std::string.
 * @return The configuration value as std::string.
 */
template <>
inline std::string CConfigValue<std::string>::operator*() const {
    return std::string{*(Hyprlang::STRING*)p_};
}

// Specializations for Hyprlang::STRING

/**
 * @brief Specialization of ptr() for Hyprlang::STRING.
 * @return Pointer to the Hyprlang::STRING configuration value.
 */
template <>
inline Hyprlang::STRING* CConfigValue<Hyprlang::STRING>::ptr() const {
    return (Hyprlang::STRING*)p_;
}

/**
 * @brief Specialization of operator* for Hyprlang::STRING.
 * @return The Hyprlang::STRING configuration value.
 */
template <>
inline Hyprlang::STRING CConfigValue<Hyprlang::STRING>::operator*() const {
    return *(Hyprlang::STRING*)p_;
}

// Specializations for Hyprlang::CUSTOMTYPE

/**
 * @brief Specialization of ptr() for Hyprlang::CUSTOMTYPE.
 * @return Pointer to the Hyprlang::CUSTOMTYPE configuration value.
 */
template <>
inline Hyprlang::CUSTOMTYPE* CConfigValue<Hyprlang::CUSTOMTYPE>::ptr() const {
    return *(Hyprlang::CUSTOMTYPE* const*)p_;
}

/**
 * @brief Specialization of operator* for Hyprlang::CUSTOMTYPE.
 * @return The Hyprlang::CUSTOMTYPE configuration value.
 */
template <>
inline Hyprlang::CUSTOMTYPE CConfigValue<Hyprlang::CUSTOMTYPE>::operator*() const {
    RASSERT(false, "Impossible to implement operator* of CConfigValue<Hyprlang::CUSTOMTYPE>, use ptr()");
    return *ptr();
}