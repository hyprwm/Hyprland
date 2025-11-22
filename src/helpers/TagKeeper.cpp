#include "TagKeeper.hpp"

bool CTagKeeper::isTagged(const std::string& tag, bool strict) const {
    const bool NEGATIVE = tag.starts_with("negative");
    const auto MATCH    = NEGATIVE ? tag.substr(9) : tag;
    const bool TAGGED   = m_tags.contains(MATCH) || (!strict && m_tags.contains(MATCH + "*"));
    return NEGATIVE ? !TAGGED : TAGGED;
}

bool CTagKeeper::applyTag(const std::string& tag, bool dynamic) {

    std::string tagReal = tag;

    if (dynamic && !tag.ends_with("*"))
        tagReal += "*";

    bool changed = true;
    bool setTag  = true;

    if (tagReal.starts_with("-")) { // unset
        tagReal = tagReal.substr(1);
        changed = isTagged(tagReal, true);
        setTag  = false;
    } else if (tagReal.starts_with("+")) { // set
        tagReal = tagReal.substr(1);
        changed = !isTagged(tagReal, true);
    } else // toggle if without prefix
        setTag = !isTagged(tagReal, true);

    if (!changed)
        return false;

    if (setTag)
        m_tags.emplace(tagReal);
    else
        m_tags.erase(tagReal);

    return true;
}

bool CTagKeeper::removeDynamicTag(const std::string& s) {
    return std::erase_if(m_tags, [&s](const auto& tag) { return tag == s + "*"; });
}
