#pragma once

#include "Target.hpp"

#include "../../desktop/view/Window.hpp"

namespace Layout {
    class CWindowTarget : public ITarget {
      public:
        static SP<CWindowTarget> create(PHLWINDOW w);
        virtual ~CWindowTarget() = default;

        virtual eTargetType type();

        virtual void        setPosition(const CBox& box);
        virtual void        assignToSpace(const SP<CSpace>& space);

        virtual bool        shouldBeFloated();

      private:
        CWindowTarget(PHLWINDOW w);

        PHLWINDOWREF m_window;
    };
};