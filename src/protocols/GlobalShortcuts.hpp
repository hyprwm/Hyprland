#pragma once
#include "../defines.hpp"
#include "hyprland-global-shortcuts-v1-protocol.h"
#include <vector>

struct SShortcut {
    wl_resource* resource;
    std::string  id, description, appid;
    uint32_t     shortcut = 0;
};

struct SShortcutClient {
    wl_client*                              client = nullptr;
    std::vector<std::unique_ptr<SShortcut>> shortcuts;
};

class CGlobalShortcutsProtocolManager;
struct CGlobalShortcutsProtocolManagerDestroyWrapper {
    wl_listener                      listener;
    CGlobalShortcutsProtocolManager* parent = nullptr;
};

class CGlobalShortcutsProtocolManager {
  public:
    CGlobalShortcutsProtocolManager();
    ~CGlobalShortcutsProtocolManager();

    void                   bindManager(wl_client* client, void* data, uint32_t version, uint32_t id);
    void                   displayDestroy();

    void                   registerShortcut(wl_client* client, wl_resource* resource, uint32_t shortcut, const char* id, const char* app_id, const char* description,
                                            const char* trigger_description);
    void                   destroyShortcut(wl_resource* resource);

    bool                   globalShortcutExists(std::string appid, std::string trigger);
    void                   sendGlobalShortcutEvent(std::string appid, std::string trigger, bool pressed);

    std::vector<SShortcut> getAllShortcuts();

    CGlobalShortcutsProtocolManagerDestroyWrapper m_liDisplayDestroy;

  private:
    std::vector<std::unique_ptr<SShortcutClient>> m_vClients;

    SShortcutClient*                              clientFromWlClient(wl_client* client);

    wl_global*                                    m_pGlobal = nullptr;
};
