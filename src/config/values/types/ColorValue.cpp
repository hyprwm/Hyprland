#include "ColorValue.hpp"

using namespace Config::Values;

CColorValue::CColorValue(const char* name, const char* description, Config::INTEGER def) : m_default(def) {
    m_name        = name;
    m_description = description;
}

const std::type_info* CColorValue::underlying() const {
    return &typeid(decltype(m_default));
}

void CColorValue::commence() {
    m_val = CConfigValue<Config::INTEGER>(m_name);
}

Config::INTEGER CColorValue::value() const {
    if (!m_val.good())
        return m_default;

    return *m_val;
}

Config::INTEGER CColorValue::defaultVal() const {
    return m_default;
}