#pragma once

#define WLR_USE_UNSTABLE

#include <hyprland/src/layout/IHyprLayout.hpp>

struct SWindowData {
    CWindow* pWindow = nullptr;
};

class CHyprCustomLayout : public IHyprLayout {
  public:
    virtual void                     onWindowCreatedTiling(CWindow*);
    virtual void                     onWindowRemovedTiling(CWindow*);
    virtual bool                     isWindowTiled(CWindow*);
    virtual void                     recalculateMonitor(const int&);
    virtual void                     recalculateWindow(CWindow*);
    virtual void                     resizeActiveWindow(const Vector2D&, CWindow* pWindow = nullptr);
    virtual void                     fullscreenRequestForWindow(CWindow*, eFullscreenMode, bool);
    virtual std::any                 layoutMessage(SLayoutMessageHeader, std::string);
    virtual SWindowRenderLayoutHints requestRenderHints(CWindow*);
    virtual void                     switchWindows(CWindow*, CWindow*);
    virtual void                     alterSplitRatio(CWindow*, float, bool);
    virtual std::string              getLayoutName();
    virtual void                     replaceWindowDataWith(CWindow* from, CWindow* to);

    virtual void                     onEnable();
    virtual void                     onDisable();

  private:
    std::vector<SWindowData> m_vWindowData;
};