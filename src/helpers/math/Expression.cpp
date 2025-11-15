#include "Expression.hpp"
#include "muParser.h"
#include "../../debug/Log.hpp"

using namespace Math;

CExpression::CExpression() : m_parser(makeUnique<mu::Parser>()) {
    ;
}

void CExpression::addVariable(const std::string& name, double val) {
    m_parser->DefineConst(name, val);
}

std::optional<double> CExpression::compute(const std::string& expr) {
    try {
        m_parser->SetExpr(expr);
        return m_parser->Eval();
    } catch (mu::Parser::exception_type& e) { Debug::log(ERR, "CExpression::compute: mu threw: {}", e.GetMsg()); }

    return std::nullopt;
}
