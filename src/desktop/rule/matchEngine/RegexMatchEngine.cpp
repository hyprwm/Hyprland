#include "RegexMatchEngine.hpp"
#include <re2/re2.h>

using namespace Desktop::Rule;

CRegexMatchEngine::CRegexMatchEngine(const std::string& regex) {
    if (regex.starts_with("negative:")) {
        m_negative = true;
        m_regex    = makeUnique<re2::RE2>(regex.substr(9));
        return;
    }
    m_regex = makeUnique<re2::RE2>(regex);
}

bool CRegexMatchEngine::match(const std::string& other) {
    return re2::RE2::FullMatch(other, *m_regex) != m_negative;
}