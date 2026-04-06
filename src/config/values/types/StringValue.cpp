#include "StringValue.hpp"

using namespace Config::Values;

CStringValue::CStringValue(const char* name, const char* description, Config::STRING def,
                           std::optional<std::function<std::expected<void, std::string>(const std::string&)>>&& validator) : m_validator(std::move(validator)), m_default(def) {
    m_name        = name;
    m_description = description;
}

const std::type_info* CStringValue::underlying() const {
    return &typeid(decltype(m_default));
}

void CStringValue::commence() {
    m_val = CConfigValue<Config::STRING>(m_name);
}

Config::STRING CStringValue::value() const {
    if (!m_val.good())
        return m_default;

    return *m_val;
}

Config::STRING CStringValue::defaultVal() const {
    return m_default;
}
