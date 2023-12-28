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

void refreshGroupBarGradients();

class CHyprGroupBarDecoration : public IHyprWindowDecoration {
  public:
    CHyprGroupBarDecoration(CWindow*);
    virtual ~CHyprGroupBarDecoration();

    virtual SDecorationPositioningInfo getPositioningInfo();

    virtual void                       onPositioningReply(const SDecorationPositioningReply& reply);

    virtual void                       draw(CMonitor*, float a, const Vector2D& offset);

    virtual eDecorationType            getDecorationType();

    virtual void                       updateWindow(CWindow*);

    virtual void                       damageEntire();

    virtual bool                       onInputOnDeco(const eInputType, const Vector2D&, std::any = {});

    virtual eDecorationLayer           getDecorationLayer();

    virtual uint64_t                   getDecorationFlags();

    virtual std::string                getDisplayName();

  private:
    SWindowDecorationExtents m_seExtents;

    CBox                     m_bAssignedBox = {0};

    CWindow*                 m_pWindow = nullptr;

    std::deque<CWindow*>     m_dwGroupMembers;

    float                    m_fBarWidth;

    CTitleTex*               textureFromTitle(const std::string&);
    void                     invalidateTextures();

    CBox                     assignedBoxGlobal();

    bool                     onBeginWindowDragOnDeco(const Vector2D&);
    bool                     onEndWindowDragOnDeco(const Vector2D&, CWindow*);
    bool                     onMouseButtonOnDeco(const Vector2D&, wlr_pointer_button_event*);
    bool                     onScrollOnDeco(const Vector2D&, wlr_pointer_axis_event*);

    struct STitleTexs {
        // STitleTexs*                            overriden = nullptr; // TODO: make shit shared in-group to decrease VRAM usage.
        std::deque<std::unique_ptr<CTitleTex>> titleTexs;
    } m_sTitleTexs;
};
