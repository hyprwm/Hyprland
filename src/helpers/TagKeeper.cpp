#include "TagKeeper.hpp"

bool CTagKeeper::isTagged(const std::string& tag, bool strict) {
    return m_tags.contains(tag) || (!strict && m_tags.contains(tag + "*"));
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

bool CTagKeeper::removeDynamicTags() {
    return std::erase_if(m_tags, [](const auto& tag) { return tag.ends_with("*"); });
}
