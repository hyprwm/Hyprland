#include "IValue.hpp"

using namespace Config::Values;
using namespace Config;

IValue::IValue(Supplementary::PropRefreshBits refreshProps) : m_refreshProps(refreshProps) {
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
