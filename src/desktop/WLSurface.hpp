#pragma once

#include "../defines.hpp"
#include "../helpers/math/Math.hpp"
#include "../helpers/signal/Signal.hpp"

class CSubsurface;
class CPopup;
class CPointerConstraint;
class CWLSurfaceResource;

class CWLSurface {
  public:
    static SP<CWLSurface> create() {
        auto p  = SP<CWLSurface>(new CWLSurface);
        p->self = p;
        return p;
    }
    ~CWLSurface();

    // anonymous surfaces are non-desktop components, e.g. a cursor surface or a DnD
    void assign(SP<CWLSurfaceResource> pSurface);
    void assign(SP<CWLSurfaceResource> pSurface, PHLWINDOW pOwner);
    void assign(SP<CWLSurfaceResource> pSurface, PHLLS pOwner);
    void assign(SP<CWLSurfaceResource> pSurface, CSubsurface* pOwner);
    void assign(SP<CWLSurfaceResource> pSurface, CPopup* pOwner);
    void unassign();

    CWLSurface(const CWLSurface&)                       = delete;
    CWLSurface(CWLSurface&&)                            = delete;
    CWLSurface&            operator=(const CWLSurface&) = delete;
    CWLSurface&            operator=(CWLSurface&&)      = delete;

    SP<CWLSurfaceResource> resource() const;
    bool                   exists() const;
    bool                   small() const;              // means surface is smaller than the requested size
    Vector2D               correctSmallVec() const;    // returns a corrective vector for small() surfaces
    Vector2D               correctSmallVecBuf() const; // returns a corrective vector for small() surfaces, in BL coords
    Vector2D               getViewporterCorrectedSize() const;
    CRegion                computeDamage() const; // logical coordinates. May be wrong if the surface is unassigned
    bool                   visible();
    bool                   keyboardFocusable() const;

    // getters for owners.
    PHLWINDOW    getWindow() const;
    PHLLS        getLayer() const;
    CPopup*      getPopup() const;
    CSubsurface* getSubsurface() const;

    // desktop components misc utils
    std::optional<CBox>    getSurfaceBoxGlobal() const;
    void                   appendConstraint(WP<CPointerConstraint> constraint);
    SP<CPointerConstraint> constraint() const;

    // allow stretching. Useful for plugins.
    bool m_bFillIgnoreSmall = false;

    // track surface data and avoid dupes
    float               m_fLastScale     = 0;
    int                 m_iLastScale     = 0;
    wl_output_transform m_eLastTransform = (wl_output_transform)-1;

    //
    CWLSurface& operator=(SP<CWLSurfaceResource> pSurface) {
        destroy();
        m_pResource = pSurface;
        init();

        return *this;
    }

    bool operator==(const CWLSurface& other) const {
        return other.resource() == resource();
    }

    bool operator==(const SP<CWLSurfaceResource> other) const {
        return other == resource();
    }

    explicit operator bool() const {
        return exists();
    }

    static SP<CWLSurface> fromResource(SP<CWLSurfaceResource> pSurface);

    // used by the alpha-modifier protocol
    float m_pAlphaModifier = 1.F;

    // used by the hyprland-surface protocol
    float m_fOverallOpacity = 1.F;

    struct {
        CSignal destroy;
    } events;

    WP<CWLSurface> self;

  private:
    CWLSurface() = default;

    bool                   m_bInert = true;

    WP<CWLSurfaceResource> m_pResource;

    PHLWINDOWREF           m_pWindowOwner;
    PHLLSREF               m_pLayerOwner;
    CPopup*                m_pPopupOwner      = nullptr;
    CSubsurface*           m_pSubsurfaceOwner = nullptr;

    //
    WP<CPointerConstraint> m_pConstraint;

    void                   destroy();
    void                   init();
    bool                   desktopComponent() const;

    struct {
        CHyprSignalListener destroy;
    } listeners;

    friend class CPointerConstraint;
};
