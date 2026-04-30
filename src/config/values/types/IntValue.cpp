#include "IntValue.hpp"

using namespace Config::Values;

CIntValue::CIntValue(const char* name, const char* description, Config::INTEGER def, SIntValueOptions&& options) :
    IValue(options.refresh), m_min(options.min), m_max(options.max), m_map(std::move(options.map)), m_default(def) {
    m_name        = name;
    m_description = description;
}

const std::type_info* CIntValue::underlying() const {
    return &typeid(decltype(m_default));
}

void CIntValue::commence() {
    m_val = CConfigValue<Config::INTEGER>(m_name);
}

Config::INTEGER CIntValue::value() const {
    if (!m_val.good())
        return m_default;

    return *m_val;
}

Config::INTEGER CIntValue::defaultVal() const {
    return m_default;
}
