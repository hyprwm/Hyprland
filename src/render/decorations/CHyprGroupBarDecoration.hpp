#pragma once

#include "IHyprWindowDecoration.hpp"
#include <deque>
#include "../Texture.hpp"
#include <string>
#include <memory>

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

    virtual void                     onBeginWindowDragOnDeco(const Vector2D&);

    virtual bool                     onEndWindowDragOnDeco(CWindow* pDraggedWindow, const Vector2D&);

    virtual void                     onMouseButtonOnDeco(const Vector2D&, wlr_pointer_button_event*);

    virtual eDecorationLayer         getDecorationLayer();

    virtual uint64_t                 getDecorationFlags();

  private:
    SWindowDecorationExtents m_seExtents;

    CWindow*                 m_pWindow = nullptr;

    Vector2D                 m_vLastWindowPos;
    Vector2D                 m_vLastWindowSize;

    std::deque<CWindow*>     m_dwGroupMembers;

    float                    m_fBarWidth;

    CTitleTex*               textureFromTitle(const std::string&);
    void                     invalidateTextures();

    void                     refreshGradients();

    struct STitleTexs {
        // STitleTexs*                            overriden = nullptr; // TODO: make shit shared in-group to decrease VRAM usage.
        std::deque<std::unique_ptr<CTitleTex>> titleTexs;
    } m_sTitleTexs;
};
