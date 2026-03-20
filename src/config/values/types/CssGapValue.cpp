#include "CssGapValue.hpp"

using namespace Config::Values;

CCssGapValue::CCssGapValue(const char* name, const char* description, Config::INTEGER def, std::optional<int64_t> min, std::optional<int64_t> max) :
    m_min(min), m_max(max), m_default(def) {
    m_name        = name;
    m_description = description;
}

const std::type_info* CCssGapValue::underlying() const {
    return &typeid(decltype(m_default));
}

void CCssGapValue::commence() {
    m_val = CConfigValue<Config::IComplexConfigValue>(m_name);
}

const Config::CCssGapData& CCssGapValue::value() const {
    if (!m_val.good())
        return m_default;

    return *dc<Config::CCssGapData*>(m_val.ptr());
}

const Config::CCssGapData& CCssGapValue::defaultVal() const {
    return m_default;
}
