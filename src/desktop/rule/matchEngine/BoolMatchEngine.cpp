#include "BoolMatchEngine.hpp"
#include "../../../helpers/Config.hpp"

using namespace Desktop::Rule;

CBoolMatchEngine::CBoolMatchEngine(const std::string& s) : m_value(truthy(s)) {
    ;
}

bool CBoolMatchEngine::match(bool other) {
    return other == m_value;
}
