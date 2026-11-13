#include "ExpressionVec2Value.hpp"

using namespace Config::Values;

CExpressionVec2Value::CExpressionVec2Value(const char* name, const char* description, Math::SExpressionVec2 def, SExpressionVec2ValueOptions&& options) :
    IValue(options.refresh, options.deprecationNotice), m_default(std::move(def)) {
    m_name        = name;
    m_description = description;
}

const std::type_info* CExpressionVec2Value::underlying() const {
    return &typeid(decltype(m_default));
}

void CExpressionVec2Value::commence() {
    ;
}

const Math::SExpressionVec2& CExpressionVec2Value::defaultVal() const {
    return m_default;
}
