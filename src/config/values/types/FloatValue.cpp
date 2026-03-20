#include "FloatValue.hpp"

using namespace Config::Values;

CFloatValue::CFloatValue(const char* name, const char* description, Config::FLOAT def, std::optional<Config::FLOAT> min, std::optional<Config::FLOAT> max) :
    m_min(min), m_max(max), m_default(def) {
    m_name        = name;
    m_description = description;
}

const std::type_info* CFloatValue::underlying() const {
    return &typeid(decltype(m_default));
}

void CFloatValue::commence() {
    m_val = CConfigValue<Config::FLOAT>(m_name);
}

Config::FLOAT CFloatValue::value() const {
    if (!m_val.good())
        return false;

    return *m_val;
}

Config::FLOAT CFloatValue::defaultVal() const {
    return m_default;
}
