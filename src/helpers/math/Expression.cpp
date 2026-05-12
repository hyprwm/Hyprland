#include "Expression.hpp"
#include "muParser.h"
#include "../../debug/log/Logger.hpp"

#include <cctype>
#include <format>

using namespace Math;

static std::string_view trimExprView(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
        s.remove_prefix(1);

    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
        s.remove_suffix(1);

    return s;
}

bool SExpressionVec2::empty() const {
    return x.empty() || y.empty();
}

std::string SExpressionVec2::toString() const {
    if (empty())
        return "";

    return std::format("{} {}", x, y);
}

std::expected<SExpressionVec2, std::string> Math::parseExpressionVec2(std::string_view raw) {
    raw = trimExprView(raw);
    if (raw.empty())
        return SExpressionVec2{};

    const auto spacePos = raw.find_first_of(" \t\n\r\f\v");
    if (spacePos == std::string_view::npos)
        return std::unexpected("expression vec2 requires two expressions separated by whitespace");

    auto lhs = trimExprView(raw.substr(0, spacePos));
    auto rhs = trimExprView(raw.substr(spacePos + 1));

    if (lhs.empty() || rhs.empty())
        return std::unexpected("expression vec2 requires two non-empty expressions");

    return SExpressionVec2{std::string{lhs}, std::string{rhs}};
}

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
    } catch (mu::Parser::exception_type& e) { Log::logger->log(Log::ERR, "CExpression::compute: mu threw: {}", e.GetMsg()); }

    return std::nullopt;
}
