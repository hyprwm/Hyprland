#include "BoolValue.hpp"

using namespace Config::Values;

CBoolValue::CBoolValue(const char* name, const char* description, bool def) : m_default(def) {
    m_name        = name;
    m_description = description;
}

const std::type_info* CBoolValue::underlying() const {
    return &typeid(decltype(m_default));
}

bool CBoolValue::value() const {
    if (!m_val.good())
        return false;

    return *m_val;
}

void CBoolValue::commence() {
    m_val = CConfigValue<Config::BOOL>(m_name);
}

Config::BOOL CBoolValue::defaultVal() const {
    return m_default;
}
