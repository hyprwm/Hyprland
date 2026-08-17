#pragma once

#include "../../DesktopTypes.hpp"

#include <cstdint>
#include <string_view>

namespace Desktop::View {
    class CGroup;
    class CWindow;

    enum eGroupRules : uint8_t {
        // effective only during first map, except for _ALWAYS variant
        GROUP_NONE        = 0,
        GROUP_SET         = 1 << 0, // Open as new group or add to focused group
        GROUP_SET_ALWAYS  = 1 << 1,
        GROUP_BARRED      = 1 << 2, // Don't insert to focused group.
        GROUP_LOCK        = 1 << 3,
        GROUP_LOCK_ALWAYS = 1 << 4,
        GROUP_INVADE      = 1 << 5, // Force enter a group, even if lock is engaged
        GROUP_OVERRIDE    = 1 << 6, // Override other rules
        GROUP_DENY        = 1 << 7,
    };

    uint16_t parseGroupRules(std::string_view rule, uint16_t currentRules = GROUP_NONE);

    class CWindowGroupMembership {
      public:
        explicit CWindowGroupMembership(CWindow& window);

        const SP<CGroup>& group() const;
        uint16_t          rules() const;

        void              applyRule(std::string_view rule);
        bool              canBeGroupedInto(const SP<CGroup>& group) const;

      private:
        friend class CGroup;

        void       attach(const SP<CGroup>& group);
        void       detach();

        CWindow&   m_window;
        SP<CGroup> m_group;
        uint16_t   m_rules = GROUP_NONE;
    };
}
