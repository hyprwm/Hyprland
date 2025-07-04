#pragma once

#include "desktop/Window.hpp"

class CWindowStack {
  public:
    CWindowStack() = default;

    void registerListeners();

    // window stack
    void                             add(PHLWINDOW window);
    void                             addFadingOut(PHLWINDOW window);
    void                             clear();
    void                             moveToZ(PHLWINDOW w, bool top);
    void                             removeSafe(PHLWINDOW window);
    const std::vector<PHLWINDOW>&    windows();
    const std::vector<PHLWINDOWREF>& windowsFadingOut();

    // render stack
    struct SRenderWindow {
        bool      rendered = false;
        PHLWINDOW window;
    };

    const std::vector<SRenderWindow>& renderWindows();

  private:
    struct {
        CHyprSignalListener windowRendered;
        CHyprSignalListener endOfRendering;
    } m_events;

    void                       markWindowRendered(PHLWINDOW window);
    void                       resetRenderWindowStates();

    std::vector<PHLWINDOW>     m_windows;
    std::vector<PHLWINDOWREF>  m_windowsFadingOut;
    std::vector<SRenderWindow> m_renderWindows;
};
