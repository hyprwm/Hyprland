#pragma once

#include "../defines.hpp"

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

    CWLSurface&  operator=(wlr_surface* pSurface) {
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

  private:
    wlr_surface* m_pWLRSurface = nullptr;

    void         destroy();
    void         init();

    DYNLISTENER(destroy);
};