#include "TagMatchEngine.hpp"
#include "../../../helpers/TagKeeper.hpp"
#include <hyprutils/string/String.hpp>
#include <hyprutils/string/VarList2.hpp>
#include <string>

using namespace Desktop::Rule;

CTagMatchEngine::CTagMatchEngine(const std::string& tag) {
    Hyprutils::String::CVarList2 tagsList(tag, 0, '+', true);
    for (const auto& t : tagsList) {
        m_tags.emplace_back(Hyprutils::String::trim(t));
    }
}

bool CTagMatchEngine::match(const CTagKeeper& keeper) {
    if (m_tags.empty())
        return false;

    for (const auto& tag : m_tags) {
        if (!keeper.isTagged(tag))
            return false;
    }

    return true;
}
