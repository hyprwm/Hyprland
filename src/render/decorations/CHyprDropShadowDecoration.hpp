#pragma once

#include "IHyprWindowDecoration.hpp"

class CHyprDropShadowDecoration : public IHyprWindowDecoration {
public:
    CHyprDropShadowDecoration(CWindow*);
    virtual ~CHyprDropShadowDecoration();

    virtual SWindowDecorationExtents getWindowDecorationExtents();

    virtual void draw(SMonitor*, float a);

    virtual eDecorationType getDecorationType();

    virtual void updateWindow(CWindow*);

    virtual void damageEntire();

private:
    SWindowDecorationExtents    m_seExtents;

    CWindow*                    m_pWindow = nullptr;

    Vector2D                    m_vLastWindowPos;
    Vector2D                    m_vLastWindowSize;
};