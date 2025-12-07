#include "TagMatchEngine.hpp"
#include "../../../helpers/TagKeeper.hpp"

using namespace Desktop::Rule;

CTagMatchEngine::CTagMatchEngine(const std::string& tag) : m_tag(tag) {
    ;
}

bool CTagMatchEngine::match(const CTagKeeper& keeper) {
    return keeper.isTagged(m_tag);
}