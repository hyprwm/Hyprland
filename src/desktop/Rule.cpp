#include "Rule.hpp"
#include <re2/re2.h>

CRuleRegexContainer::CRuleRegexContainer(const std::string& regex_) {
    const bool NEGATIVE = regex_.starts_with("negative:");

    negative = NEGATIVE;
    regex    = std::make_unique<RE2>(NEGATIVE ? regex_.substr(9) : regex_);
}

bool CRuleRegexContainer::passes(const std::string& str) const {
    if (!regex)
        return false;

    return RE2::FullMatch(str, *regex) != negative;
}