#include "CornersValue.hpp"

using namespace Config::Values;

CCornersValue::CCornersValue(const char* name, const char* description, Config::INTEGER def, SCornersValueOptions&& options) :
    IValue(options.refresh, options.deprecationNotice), m_min(options.min), m_max(options.max), m_default(def) {
    m_name        = name;
    m_description = description;
}

const std::type_info* CCornersValue::underlying() const {
    return &typeid(decltype(m_default));
}

void CCornersValue::commence() {
    m_val = CConfigValue<Config::IComplexConfigValue>(m_name);
}

const Config::CCornersData& CCornersValue::value() const {
    if (!m_val.good())
        return m_default;

    return *dc<Config::CCornersData*>(m_val.ptr());
}

const Config::CCornersData& CCornersValue::defaultVal() const {
    return m_default;
}
