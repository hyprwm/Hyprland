#include "Vec2Value.hpp"

using namespace Config::Values;

CVec2Value::CVec2Value(const char* name, const char* description, Config::VEC2 def,
                       std::optional<std::function<std::expected<void, std::string>(const Config::VEC2&)>>&& validator) : m_validator(std::move(validator)), m_default(def) {
    m_name        = name;
    m_description = description;
}

const std::type_info* CVec2Value::underlying() const {
    return &typeid(decltype(m_default));
}

void CVec2Value::commence() {
    m_val = CConfigValue<Config::VEC2>(m_name);
}

Config::VEC2 CVec2Value::value() const {
    if (!m_val.good())
        return m_default;

    return *m_val;
}

Config::VEC2 CVec2Value::defaultVal() const {
    return m_default;
}
