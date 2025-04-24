#include <re2/re2.h>
#include "../helpers/memory/Memory.hpp"
#include "Rule.hpp"
#include "../debug/Log.hpp"

CRuleRegexContainer::CRuleRegexContainer(const std::string& regex_) {
    const bool NEGATIVE = regex_.starts_with("negative:");

    m_negative = NEGATIVE;
    m_regex    = makeUnique<RE2>(NEGATIVE ? regex_.substr(9) : regex_);

    // TODO: maybe pop an error?
    if (!m_regex->ok())
        Debug::log(ERR, "RuleRegexContainer: regex {} failed to parse!", regex_);
}

bool CRuleRegexContainer::passes(const std::string& str) const {
    if (!m_regex)
        return false;

    return RE2::FullMatch(str, *m_regex) != m_negative;
}