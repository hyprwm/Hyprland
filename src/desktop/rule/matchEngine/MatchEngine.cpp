#include "MatchEngine.hpp"

using namespace Desktop::Rule;

bool IMatchEngine::match(const std::string&) {
    return false;
}

bool IMatchEngine::match(bool) {
    return false;
}

bool IMatchEngine::match(int) {
    return false;
}

bool IMatchEngine::match(PHLWORKSPACE) {
    return false;
}

bool IMatchEngine::match(const CTagKeeper& keeper) {
    return false;
}
