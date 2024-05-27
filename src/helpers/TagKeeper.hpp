#pragma once

#include <string>
#include <set>

class CTagKeeper {
  public:
    bool               isTagged(const std::string& tag, bool strict = false);
    bool               applyTag(const std::string& tag, bool dynamic = false);
    bool               removeDynamicTags();

    inline const auto& getTags() {
        return m_tags;
    };

  private:
    std::set<std::string> m_tags;
};
