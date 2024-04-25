#pragma once

#include <memory>
#include <vector>
#include <cstdint>
#include "WaylandProtocol.hpp"
#include "keyboard-shortcuts-inhibit-unstable-v1.hpp"

class CKeyboardShortcutsInhibitor {
  public:
    CKeyboardShortcutsInhibitor(SP<CZwpKeyboardShortcutsInhibitorV1> resource_, wlr_surface* surf);

    // read-only pointer, may be invalid
    wlr_surface* surface();
    bool         good();

  private:
    SP<CZwpKeyboardShortcutsInhibitorV1> resource;
    wlr_surface*                         pSurface = nullptr;
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
    std::vector<UP<CZwpKeyboardShortcutsInhibitManagerV1>> m_vManagers;
    std::vector<UP<CKeyboardShortcutsInhibitor>>           m_vInhibitors;

    friend class CKeyboardShortcutsInhibitor;
};

namespace PROTO {
    inline UP<CKeyboardShortcutsInhibitProtocol> shortcutsInhibit;
};