#pragma once
#include "../defines.hpp"
#include "hyprland-global-shortcuts-v1.hpp"
#include "../protocols/WaylandProtocol.hpp"
#include <vector>

struct SShortcut {
    SP<CHyprlandGlobalShortcutV1> resource;
    std::string                   id, description, appid;
    uint32_t                      shortcut = 0;
};

class CShortcutClient {
  public:
    CShortcutClient(SP<CHyprlandGlobalShortcutsManagerV1> resource);

    bool good();

  private:
    SP<CHyprlandGlobalShortcutsManagerV1> m_resource;
    std::vector<SP<SShortcut>>            m_shortcuts;

    friend class CGlobalShortcutsProtocol;
};

class CGlobalShortcutsProtocol : IWaylandProtocol {
  public:
    CGlobalShortcutsProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    void                   bindManager(wl_client* client, void* data, uint32_t version, uint32_t id);
    void                   destroyResource(CShortcutClient* client);

    void                   sendGlobalShortcutEvent(std::string appid, std::string trigger, bool pressed);
    bool                   isTaken(std::string id, std::string app_id);
    std::vector<SShortcut> getAllShortcuts();

  private:
    std::vector<SP<CShortcutClient>> m_clients;
};

namespace PROTO {
    inline UP<CGlobalShortcutsProtocol> globalShortcuts;
};
