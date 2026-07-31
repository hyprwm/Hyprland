#include "IValue.hpp"

using namespace Config::Values;
using namespace Config;

IValue::IValue(Supplementary::PropRefreshBits refreshProps, const char* deprecationNotice) : m_deprecationNotice(deprecationNotice), m_refreshProps(refreshProps) {
    ;
}

const char* IValue::name() const {
    return m_name;
}

const char* IValue::description() const {
    return m_description;
}

Supplementary::PropRefreshBits IValue::refreshBits() const {
    return m_refreshProps;
}

std::optional<const char*> IValue::deprecationNotice() const {
    return m_deprecationNotice ? std::optional<const char*>{m_deprecationNotice} : std::nullopt;
}
