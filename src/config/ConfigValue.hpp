#pragma once

#include <string>
#include <typeindex>
#include <typeinfo>
#include <hyprlang.hpp>
#include "../macros.hpp"
#include "../config/shared/complex/ComplexDataType.hpp"
#include "../config/shared/Types.hpp"

// Welcome to wonky fucking pointer + type hell
// Enjoy your stay

// giga hack to avoid including configManager here
// NOLINTNEXTLINE
void            local__configValuePopulate(void* const** p, void* const** hlangp, std::type_index* ti, const std::string& val);
std::type_index local__configValueTypeIdx(const std::string& val);

template <typename T>
class CConfigValue {
  public:
    // creates an empty value. Deref'ing this will be a crash
    CConfigValue() = default;

    CConfigValue(const std::string& val) {
#ifdef HYPRLAND_DEBUG
        // verify type
        // TODO: fix this or leave it idk I'm tired.
        // const auto TYPE = local__configValueTypeIdx(val);

        // // exceptions
        // const bool STRINGEX = (typeid(T) == typeid(std::string) && TYPE == typeid(Hyprlang::STRING));
        // const bool CUSTOMEX = ((typeid(T) == typeid(Hyprlang::CUSTOMTYPE) || typeid(T) == typeid(Config::IComplexConfigValue)) &&
        //                        (TYPE == typeid(Hyprlang::CUSTOMTYPE*) || TYPE == typeid(Config::IComplexConfigValue*) || TYPE == typeid(void*) /* dunno why it does this? */));

        // RASSERT(typeid(T) == TYPE || STRINGEX || CUSTOMEX, "Mismatched type in CConfigValue<T>, got {} but has {}", typeid(T).name(), TYPE.name());
#endif

        local__configValuePopulate(&m_p, &m_hlangp, &m_typeIndex, val);
    }

    T* ptr() const {
        return *rc<T* const*>(m_p);
    }

    T operator*() const {
        return *ptr();
    }

    bool good() const {
        return m_p || m_hlangp;
    }

  private:
    void* const*    m_p         = nullptr;
    void* const*    m_hlangp    = nullptr;
    std::type_index m_typeIndex = typeid(void);
};

template <>
inline std::string* CConfigValue<std::string>::ptr() const {
    RASSERT(false, "Impossible to implement ptr() of CConfigValue<std::string>");
    return nullptr;
}

template <>
inline std::string CConfigValue<std::string>::operator*() const {
    if (m_typeIndex == typeid(std::string))
        return **rc<const std::string* const*>(m_p);
    else if (m_typeIndex == typeid(const char*))
        return std::string{*rc<const Hyprlang::STRING*>(m_hlangp)};
    else
        RASSERT(false, "CConfigValue<std::string> on a FUCKED type");
    return "FUCK";
}

template <>
inline Config::INTEGER CConfigValue<Config::INTEGER>::operator*() const {
    if (m_typeIndex == typeid(bool))
        return **rc<const bool* const*>(m_p);
    else if (m_typeIndex == typeid(Config::INTEGER))
        return **rc<const Config::INTEGER* const*>(m_p);
    else
        RASSERT(false, "CConfigValue<Config::INTEGER> on a FUCKED type");
    return -1;
}

template <>
inline Config::IComplexConfigValue* CConfigValue<Config::IComplexConfigValue>::ptr() const {
    if (m_hlangp)
        return rc<Config::IComplexConfigValue*>((*rc<Hyprlang::CUSTOMTYPE* const*>(m_hlangp))->getData());
    else
        return *rc<Config::IComplexConfigValue* const*>(m_p);
}
