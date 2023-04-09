#include "GlobalShortcuts.hpp"
#include "../Compositor.hpp"

#define GLOBAL_SHORTCUTS_VERSION 1

static void bindManagerInt(wl_client* client, void* data, uint32_t version, uint32_t id) {
    g_pProtocolManager->m_pGlobalShortcutsProtocolManager->bindManager(client, data, version, id);
}

static void handleDisplayDestroy(struct wl_listener* listener, void* data) {
    g_pProtocolManager->m_pGlobalShortcutsProtocolManager->displayDestroy();
}

void CGlobalShortcutsProtocolManager::displayDestroy() {
    wl_global_destroy(m_pGlobal);
}

CGlobalShortcutsProtocolManager::CGlobalShortcutsProtocolManager() {
    m_pGlobal = wl_global_create(g_pCompositor->m_sWLDisplay, &hyprland_global_shortcuts_manager_v1_interface, GLOBAL_SHORTCUTS_VERSION, this, bindManagerInt);

    if (!m_pGlobal) {
        Debug::log(ERR, "GlobalShortcutsManager could not start!");
        return;
    }

    m_liDisplayDestroy.notify = handleDisplayDestroy;
    wl_display_add_destroy_listener(g_pCompositor->m_sWLDisplay, &m_liDisplayDestroy);

    Debug::log(LOG, "GlobalShortcutsManager started successfully!");
}

static void handleRegisterShortcut(wl_client* client, wl_resource* resource, uint32_t shortcut, const char* id, const char* app_id, const char* description,
                                   const char* trigger_description) {
    g_pProtocolManager->m_pGlobalShortcutsProtocolManager->registerShortcut(client, resource, shortcut, id, app_id, description, trigger_description);
}

static void handleDestroy(wl_client* client, wl_resource* resource) {
    wl_resource_destroy(resource);
}

static const struct hyprland_global_shortcuts_manager_v1_interface globalShortcutsManagerImpl = {
    .register_shortcut = handleRegisterShortcut,
    .destroy           = handleDestroy,
};

static const struct hyprland_global_shortcut_v1_interface shortcutImpl = {
    .destroy = handleDestroy,
};

void CGlobalShortcutsProtocolManager::bindManager(wl_client* client, void* data, uint32_t version, uint32_t id) {
    const auto RESOURCE = wl_resource_create(client, &hyprland_global_shortcuts_manager_v1_interface, version, id);
    wl_resource_set_implementation(RESOURCE, &globalShortcutsManagerImpl, this, nullptr);

    Debug::log(LOG, "GlobalShortcutsManager bound successfully!");

    m_vClients.emplace_back(std::make_unique<SShortcutClient>(client));
}

SShortcutClient* CGlobalShortcutsProtocolManager::clientFromWlClient(wl_client* client) {
    for (auto& c : m_vClients) {
        if (c->client == client) {
            return c.get();
        }
    }

    return nullptr;
}

static void onShortcutDestroy(wl_resource* pResource) {
    g_pProtocolManager->m_pGlobalShortcutsProtocolManager->destroyShortcut(pResource);
}

void CGlobalShortcutsProtocolManager::registerShortcut(wl_client* client, wl_resource* resource, uint32_t shortcut, const char* id, const char* app_id, const char* description,
                                                       const char* trigger_description) {
    const auto PCLIENT = clientFromWlClient(client);

    if (!PCLIENT) {
        Debug::log(ERR, "Error at global shortcuts: no client in register?");
        return;
    }

    for (auto& c : m_vClients) {
        for (auto& sh : c->shortcuts) {
            if (sh->appid == app_id && sh->id == id) {
                wl_resource_post_error(resource, HYPRLAND_GLOBAL_SHORTCUTS_MANAGER_V1_ERROR_ALREADY_TAKEN, "Combination is taken");
                return;
            }
        }
    }

    const auto PSHORTCUT   = PCLIENT->shortcuts.emplace_back(std::make_unique<SShortcut>()).get();
    PSHORTCUT->id          = id;
    PSHORTCUT->description = description;
    PSHORTCUT->appid       = app_id;
    PSHORTCUT->shortcut    = shortcut;

    PSHORTCUT->resource = wl_resource_create(client, &hyprland_global_shortcut_v1_interface, 1, shortcut);
    if (!PSHORTCUT->resource) {
        wl_client_post_no_memory(client);
        std::erase_if(PCLIENT->shortcuts, [&](const auto& other) { return other.get() == PSHORTCUT; });
        return;
    }

    wl_resource_set_implementation(PSHORTCUT->resource, &shortcutImpl, this, &onShortcutDestroy);
}

bool CGlobalShortcutsProtocolManager::globalShortcutExists(std::string appid, std::string trigger) {
    for (auto& c : m_vClients) {
        for (auto& sh : c->shortcuts) {
            if (sh->appid == appid && sh->id == trigger) {
                return true;
            }
        }
    }

    return false;
}

void CGlobalShortcutsProtocolManager::sendGlobalShortcutEvent(std::string appid, std::string trigger, bool pressed) {
    for (auto& c : m_vClients) {
        for (auto& sh : c->shortcuts) {
            if (sh->appid == appid && sh->id == trigger) {
                timespec now;
                clock_gettime(CLOCK_MONOTONIC, &now);
                uint32_t tvSecHi = (sizeof(now.tv_sec) > 4) ? now.tv_sec >> 32 : 0;
                uint32_t tvSecLo = now.tv_sec & 0xFFFFFFFF;
                if (pressed)
                    hyprland_global_shortcut_v1_send_pressed(sh->resource, tvSecHi, tvSecLo, now.tv_nsec);
                else
                    hyprland_global_shortcut_v1_send_released(sh->resource, tvSecHi, tvSecLo, now.tv_nsec);
            }
        }
    }
}

std::vector<SShortcut> CGlobalShortcutsProtocolManager::getAllShortcuts() {
    std::vector<SShortcut> copy;
    for (auto& c : m_vClients) {
        for (auto& sh : c->shortcuts) {
            copy.push_back(*sh);
        }
    }

    return copy;
}

void CGlobalShortcutsProtocolManager::destroyShortcut(wl_resource* resource) {
    for (auto& c : m_vClients) {
        std::erase_if(c->shortcuts, [&](const auto& other) { return other->resource == resource; });
    }
}