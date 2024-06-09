#pragma once

#include "IHyprWindowDecoration.hpp"
#include "../../devices/IPointer.hpp"
#include <deque>
#include "../Texture.hpp"
#include <string>
#include <memory>

class CTitleTex {
  public:
    CTitleTex(PHLWINDOW pWindow, const Vector2D& bufferSize, const float monitorScale);
    ~CTitleTex();

    SP<CTexture> tex;
    std::string  szContent;
    int          textWidth;
    int          textHeight;

    PHLWINDOWREF pWindowOwner;
};

void refreshGroupBarGradients();

class CHyprGroupBarDecoration : public IHyprWindowDecoration {
  public:
    CHyprGroupBarDecoration(PHLWINDOW);
    virtual ~CHyprGroupBarDecoration();

    virtual SDecorationPositioningInfo getPositioningInfo();

    virtual void                       onPositioningReply(const SDecorationPositioningReply& reply);

    virtual void                       draw(CMonitor*, float a);

    virtual eDecorationType            getDecorationType();

    virtual void                       updateWindow(PHLWINDOW);

    virtual void                       damageEntire();

    virtual bool                       onInputOnDeco(const eInputType, const Vector2D&, std::any = {});

    virtual eDecorationLayer           getDecorationLayer();

    virtual uint64_t                   getDecorationFlags();

    virtual std::string                getDisplayName();

  private:
    SWindowDecorationExtents m_seExtents;

    CBox                     m_bAssignedBox = {0};

    PHLWINDOWREF             m_pWindow;

    std::deque<PHLWINDOWREF> m_dwGroupMembers;

    float                    m_fBarWidth;
    float                    m_fBarHeight;

    CTitleTex*               textureFromTitle(const std::string&);
    void                     invalidateTextures();

    CBox                     assignedBoxGlobal();

    bool                     onBeginWindowDragOnDeco(const Vector2D&);
    bool                     onEndWindowDragOnDeco(const Vector2D&, PHLWINDOW);
    bool                     onMouseButtonOnDeco(const Vector2D&, const IPointer::SButtonEvent&);
    bool                     onScrollOnDeco(const Vector2D&, const IPointer::SAxisEvent);

    struct STitleTexs {
        // STitleTexs*                            overriden = nullptr; // TODO: make shit shared in-group to decrease VRAM usage.
        std::deque<std::unique_ptr<CTitleTex>> titleTexs;
    } m_sTitleTexs;
};
