#pragma once

#include <vector>
#include <cstdint>
#include "WaylandProtocol.hpp"
#include "keyboard-shortcuts-inhibit-unstable-v1.hpp"

class CWLSurfaceResource;

class CKeyboardShortcutsInhibitor {
  public:
    CKeyboardShortcutsInhibitor(SP<CZwpKeyboardShortcutsInhibitorV1> resource_, SP<CWLSurfaceResource> surf);

    // read-only pointer, may be invalid
    SP<CWLSurfaceResource> surface();
    bool                   good();

  private:
    SP<CZwpKeyboardShortcutsInhibitorV1> m_resource;
    WP<CWLSurfaceResource>               m_surface;
};

class CKeyboardShortcutsInhibitProtocol : public IWaylandProtocol {
  public:
    CKeyboardShortcutsInhibitProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

    bool         isInhibited();

  private:
    void onManagerResourceDestroy(wl_resource* res);
    void destroyInhibitor(CKeyboardShortcutsInhibitor* pointer);
    void onInhibit(CZwpKeyboardShortcutsInhibitManagerV1* pMgr, uint32_t id, wl_resource* surface, wl_resource* seat);

    //
    std::vector<UP<CZwpKeyboardShortcutsInhibitManagerV1>> m_managers;
    std::vector<UP<CKeyboardShortcutsInhibitor>>           m_inhibitors;

    friend class CKeyboardShortcutsInhibitor;
};

namespace PROTO {
    inline UP<CKeyboardShortcutsInhibitProtocol> shortcutsInhibit;
};