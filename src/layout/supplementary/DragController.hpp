#pragma once

#include "../target/Target.hpp"
#include "../../managers/input/InputManager.hpp"

namespace Layout {
    enum eRectCorner : uint8_t;
}

namespace Layout::Supplementary {

    // DragStateController contains logic to begin and end a drag, which shouldn't be part of the layout's job. It's stuff like
    // toggling float when dragging tiled, remembering sizes, checking deltas, etc.
    class CDragStateController {
      public:
        CDragStateController()  = default;
        ~CDragStateController() = default;

        void           dragBegin(SP<ITarget> target, eMouseBindMode mode);
        void           dragEnd();

        void           mouseMove(const Vector2D& mousePos);
        eMouseBindMode mode() const;
        bool           wasDraggingWindow() const;
        bool           dragThresholdReached() const;
        void           resetDragThresholdReached();
        bool           draggingTiled() const;

        /*
            Called to try to pick up window for dragging.
            Updates drag related variables and floats window if threshold reached.
            Return true to reject
        */
        bool        updateDragWindow();

        SP<ITarget> target() const;

      private:
        WP<ITarget>         m_target;

        eMouseBindMode      m_dragMode             = MBIND_INVALID;
        bool                m_wasDraggingWindow    = false;
        bool                m_dragThresholdReached = false;
        bool                m_draggingTiled        = false;

        int                 m_mouseMoveEventCount = 0;
        Vector2D            m_beginDragXY;
        Vector2D            m_lastDragXY;
        Vector2D            m_beginDragPositionXY;
        Vector2D            m_beginDragSizeXY;
        Vector2D            m_draggingWindowOriginalFloatSize;
        Layout::eRectCorner m_grabbedCorner = sc<Layout::eRectCorner>(0) /* CORNER_NONE */;
    };
};
