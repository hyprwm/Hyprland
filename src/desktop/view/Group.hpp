#pragma once

#include "../DesktopTypes.hpp"

#include <vector>

namespace Layout {
    class CWindowGroupTarget;
};

namespace Desktop::View {
    class CGroup {
      public:
        static SP<CGroup> create(std::vector<PHLWINDOWREF>&& windows);
        ~CGroup();

        bool                             has(PHLWINDOW w) const;

        void                             add(PHLWINDOW w);
        void                             remove(PHLWINDOW w);
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

        bool                      m_locked = false;
        bool                      m_deny   = false;

        size_t                    m_current = 0;
    };

    std::vector<WP<CGroup>>& groups();
};