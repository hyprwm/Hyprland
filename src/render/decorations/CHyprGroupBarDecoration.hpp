#pragma once

#include "IHyprWindowDecoration.hpp"
#include <deque>
#include "../Texture.hpp"
#include <string>

class CTitleTex {
  public:
    CTitleTex(CWindow* pWindow, const Vector2D& bufferSize);
    ~CTitleTex();

    CTexture    tex;
    std::string szContent;
    CWindow*    pWindowOwner = nullptr;
};

class CHyprGroupBarDecoration : public IHyprWindowDecoration {
  public:
    CHyprGroupBarDecoration(CWindow*);
    virtual ~CHyprGroupBarDecoration();

    virtual SWindowDecorationExtents getWindowDecorationExtents();

    virtual void                     draw(CMonitor*, float a, const Vector2D& offset);

    virtual eDecorationType          getDecorationType();

    virtual void                     updateWindow(CWindow*);

    virtual void                     damageEntire();

    virtual SWindowDecorationExtents getWindowDecorationReservedArea();

  private:
    SWindowDecorationExtents               m_seExtents;

    CWindow*                               m_pWindow = nullptr;

    Vector2D                               m_vLastWindowPos;
    Vector2D                               m_vLastWindowSize;

    std::deque<CWindow*>                   m_dwGroupMembers;
    std::deque<std::unique_ptr<CTitleTex>> m_dpTitleTextures;

    CTitleTex*                             textureFromTitle(const std::string&);
    void                                   clearUnusedTextures();
    void                                   invalidateTextures();

    void                                   refreshGradients();
    CTexture                               m_tGradientActive;
    CTexture                               m_tGradientInactive;
};