#pragma once

#include "../defines.hpp"

class CWindow;

class CWLSurface {
  public:
    CWLSurface() = default;
    CWLSurface(wlr_surface* pSurface);
    ~CWLSurface();

    void assign(wlr_surface* pSurface);
    void unassign();

    CWLSurface(const CWLSurface&) = delete;
    CWLSurface(CWLSurface&&)      = delete;
    CWLSurface&  operator=(const CWLSurface&) = delete;
    CWLSurface&  operator=(CWLSurface&&) = delete;

    wlr_surface* wlr() const;
    bool         exists() const;
    bool         small() const;           // means surface is smaller than the requested size
    Vector2D     correctSmallVec() const; // returns a corrective vector for small() surfaces

    // allow stretching. Useful for plugins.
    bool m_bFillIgnoreSmall = false;

    // if present, means this is a base surface of a window. Cleaned on unassign()
    CWindow*    m_pOwner = nullptr;

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
        return (CWLSurface*)pSurface->data;
    }

  private:
    wlr_surface* m_pWLRSurface = nullptr;

    void         destroy();
    void         init();

    DYNLISTENER(destroy);
};