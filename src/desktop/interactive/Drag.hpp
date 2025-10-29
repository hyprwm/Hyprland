#pragma once

#include "Interactive.hpp"
#include "../../SharedDefs.hpp"

namespace Interactive {

    enum eDragInputType : uint8_t {
        DRAG_INPUT_TYPE_MOUSE = 0,
        DRAG_INPUT_TYPE_TOUCH,
    };

    class CDrag {
      public:
        ~CDrag();

        static bool            end();
        static bool            start(PHLWINDOW w, eWindowDragMode mode, eDragInputType inType = DRAG_INPUT_TYPE_MOUSE);
        static bool            active();
        static bool            dragThresholdReached();
        static void            clearDragThreshold();
        static PHLWINDOW       getDragWindow();
        static eWindowDragMode getDragMode();

      private:
        CDrag(PHLWINDOW w, eWindowDragMode mode, eDragInputType inType = DRAG_INPUT_TYPE_MOUSE);

        void                         onBeginDrag();
        bool                         updateDragWindow();
        void                         onMouseMoved();
        void                         onEndDrag();

        SP<HOOK_CALLBACK_FN>         m_mouseButton, m_mouseMove;
        SP<HOOK_CALLBACK_FN>         m_touchUp, m_touchMove;

        PHLWINDOWREF                 m_window;
        Interactive::eWindowDragMode m_dragMode             = Interactive::WINDOW_DRAG_INVALID;
        bool                         m_dragThresholdReached = false;

        int                          m_mouseMoveEventCount = 0;
        Vector2D                     m_beginDragXY;
        Vector2D                     m_lastDragXY;
        Vector2D                     m_beginDragPositionXY;
        Vector2D                     m_beginDragSizeXY;
        Vector2D                     m_draggingWindowOriginalFloatSize;
        eRectCorner                  m_grabbedCorner = CORNER_TOPLEFT;
    };
};