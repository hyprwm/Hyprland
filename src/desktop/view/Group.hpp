#pragma once

#include "../DesktopTypes.hpp"
#include "../../helpers/math/Direction.hpp"

#include <optional>
#include <vector>

namespace Layout {
    class CWindowGroupTarget;
};

namespace Desktop::View {
    class CGroup {
      public:
        static SP<CGroup> create(std::vector<PHLWINDOWREF>&& windows);
        ~CGroup();

        enum eRemoveFromGroupReason : uint8_t {
            REMOVE_FROM_GROUP_REASON_UNMAP_WINDOW,
        };

        bool                             has(PHLWINDOW w) const;

        void                             add(PHLWINDOW w);
        void                             remove(PHLWINDOW w, Math::eDirection dir = Math::DIRECTION_DEFAULT, std::optional<eRemoveFromGroupReason> reason = std::nullopt);
        void                             moveCurrent(bool next);
        void                             setCurrent(size_t idx);
        void                             setCurrent(PHLWINDOW w);
        size_t                           getCurrentIdx() const;
        size_t                           size() const;
        void                             destroy();
        void                             updateWorkspace(PHLWORKSPACE);

        void                             swapWithNext();
        void                             swapWithLast();

        PHLWINDOW                        head() const;
        PHLWINDOW                        tail() const;
        PHLWINDOW                        current() const;
        PHLWINDOW                        next() const;

        PHLWINDOW                        fromIndex(size_t idx) const;

        bool                             locked() const;
        void                             setLocked(bool x);

        bool                             denied() const;
        void                             setDenied(bool x);

        const std::vector<PHLWINDOWREF>& windows() const;

        SP<Layout::CWindowGroupTarget>   m_target;

      private:
        CGroup(std::vector<PHLWINDOWREF>&& windows);

        void                      applyWindowDecosAndUpdates(PHLWINDOW x);
        void                      removeWindowDecos(PHLWINDOW x);
        void                      init();
        void                      updateWindowVisibility();

        WP<CGroup>                m_self;

        std::vector<PHLWINDOWREF> m_windows;

        size_t                    m_current = 0;

        uint32_t                  m_groupPolicyFlags = 0;
    };

    std::vector<WP<CGroup>>& groups();
};
