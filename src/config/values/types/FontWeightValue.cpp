#include "FontWeightValue.hpp"

using namespace Config::Values;

CFontWeightValue::CFontWeightValue(const char* name, const char* description, Config::INTEGER def) : m_default(def) {
    m_name        = name;
    m_description = description;
}

const std::type_info* CFontWeightValue::underlying() const {
    return &typeid(Config::IComplexConfigValue*);
}

void CFontWeightValue::commence() {
    m_val = CConfigValue<Config::IComplexConfigValue>(m_name);
}

const Config::CFontWeightConfigValueData& CFontWeightValue::value() const {
    if (!m_val.good())
        return m_default;

    return *dc<Config::CFontWeightConfigValueData*>(m_val.ptr());
}

const Config::CFontWeightConfigValueData& CFontWeightValue::defaultVal() const {
    return m_default;
}
