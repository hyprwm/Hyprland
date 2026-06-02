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

class CConfigValueBase {
  protected:
    std::string     m_valueName;
    void* const*    m_p         = nullptr;
    void* const*    m_hlangp    = nullptr;
    std::type_index m_typeIndex = typeid(void);

    CConfigValueBase();
    ~CConfigValueBase();

    void populateFromName();
    void bindInternal(const std::string& val);

  public:
    static std::vector<CConfigValueBase*>& registry();
    static void                            flushCaches();
};

template <typename T>
class CConfigValue : private CConfigValueBase {
  public:
    // creates an empty value. Deref'ing this will be a crash
    CConfigValue() = default;
    explicit CConfigValue(const std::string& val) {
        bind(val);
    }

    void bind(const std::string& val) {
        bindInternal(val);
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
