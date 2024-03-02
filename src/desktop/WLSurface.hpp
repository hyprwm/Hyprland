#pragma once

#include "../defines.hpp"
#include "../helpers/Region.hpp"
#include "Constraint.hpp"

class CWindow;
struct SLayerSurface;
class CSubsurface;
class CPopup;

class CWLSurface {
  public:
    CWLSurface() = default;
    ~CWLSurface();

    // anonymous surfaces are non-desktop components, e.g. a cursor surface or a DnD
    void assign(wlr_surface* pSurface);
    void assign(wlr_surface* pSurface, CWindow* pOwner);
    void assign(wlr_surface* pSurface, SLayerSurface* pOwner);
    void assign(wlr_surface* pSurface, CSubsurface* pOwner);
    void assign(wlr_surface* pSurface, CPopup* pOwner);
    void unassign();

    CWLSurface(const CWLSurface&)             = delete;
    CWLSurface(CWLSurface&&)                  = delete;
    CWLSurface&  operator=(const CWLSurface&) = delete;
    CWLSurface&  operator=(CWLSurface&&)      = delete;

    wlr_surface* wlr() const;
    bool         exists() const;
    bool         small() const;           // means surface is smaller than the requested size
    Vector2D     correctSmallVec() const; // returns a corrective vector for small() surfaces
    Vector2D     getViewporterCorrectedSize() const;
    CRegion      logicalDamage() const;
    void         onCommit();

    // getters for owners.
    CWindow*       getWindow();
    SLayerSurface* getLayer();
    CPopup*        getPopup();
    CSubsurface*   getSubsurface();

    // desktop components misc utils
    std::optional<CBox> getSurfaceBoxGlobal();
    void                appendConstraint(wlr_pointer_constraint_v1* constraint);
    CConstraint*        constraint();

    // allow stretching. Useful for plugins.
    bool m_bFillIgnoreSmall = false;

    // track surface data and avoid dupes
    float               m_fLastScale     = 0;
    int                 m_iLastScale     = 0;
    wl_output_transform m_eLastTransform = (wl_output_transform)-1;

    //
    CWLSurface& operator=(wlr_surface* pSurface) {
        destroy();
        m_pWLRSurface = pSurface;
        init();

        return *this;
    }

    bool operator==(const CWLSurface& other) const {
        return other.wlr() == wlr();
    }

    bool operator==(const wlr_surface* other) const {
        return other == wlr();
    }

    explicit operator bool() const {
        return exists();
    }

    static CWLSurface* surfaceFromWlr(wlr_surface* pSurface) {
        if (!pSurface)
            return nullptr;
        return (CWLSurface*)pSurface->data;
    }

  private:
    bool           m_bInert = true;

    wlr_surface*   m_pWLRSurface = nullptr;

    CWindow*       m_pWindowOwner     = nullptr;
    SLayerSurface* m_pLayerOwner      = nullptr;
    CPopup*        m_pPopupOwner      = nullptr;
    CSubsurface*   m_pSubsurfaceOwner = nullptr;

    //
    std::unique_ptr<CConstraint> m_pConstraint;

    void                         destroy();
    void                         init();
    bool                         desktopComponent();

    DYNLISTENER(destroy);
    DYNLISTENER(commit);

    friend class CConstraint;
};