#include "IValue.hpp"

using namespace Config::Values;

const char* IValue::name() const {
    return m_name;
}

const char* IValue::description() const {
    return m_description;
}
