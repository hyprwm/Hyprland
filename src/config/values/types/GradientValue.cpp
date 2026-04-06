#include "GradientValue.hpp"

using namespace Config::Values;

CGradientValue::CGradientValue(const char* name, const char* description, CHyprColor def) : m_default(def) {
    m_name        = name;
    m_description = description;
}

const std::type_info* CGradientValue::underlying() const {
    return &typeid(decltype(m_default));
}

void CGradientValue::commence() {
    m_val = CConfigValue<Config::IComplexConfigValue>(m_name);
}

const Config::CGradientValueData& CGradientValue::value() const {
    if (!m_val.good())
        return m_default;

    return *dc<Config::CGradientValueData*>(m_val.ptr());
}

const Config::CGradientValueData& CGradientValue::defaultVal() const {
    return m_default;
}
