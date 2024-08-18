#include "GlobalShortcuts.hpp"
#include "../Compositor.hpp"

CShortcutClient::CShortcutClient(SP<CHyprlandGlobalShortcutsManagerV1> resource_) : resource(resource_) {
    if (!good())
        return;

    resource->setOnDestroy([this](CHyprlandGlobalShortcutsManagerV1* pMgr) { PROTO::globalShortcuts->destroyResource(this); });
    resource->setDestroy([this](CHyprlandGlobalShortcutsManagerV1* pMgr) { PROTO::globalShortcuts->destroyResource(this); });

    resource->setRegisterShortcut([this](CHyprlandGlobalShortcutsManagerV1* pMgr, uint32_t shortcut, const char* id, const char* app_id, const char* description,
                                         const char* trigger_description) {
        if (PROTO::globalShortcuts->isTaken(id, app_id)) {
            resource->error(HYPRLAND_GLOBAL_SHORTCUTS_MANAGER_V1_ERROR_ALREADY_TAKEN, "Combination is taken");
            return;
        }

        const auto PSHORTCUT   = shortcuts.emplace_back(makeShared<SShortcut>(makeShared<CHyprlandGlobalShortcutV1>(resource->client(), resource->version(), shortcut)));
        PSHORTCUT->id          = id;
        PSHORTCUT->description = description;
        PSHORTCUT->appid       = app_id;
        PSHORTCUT->shortcut    = shortcut;

        if (!PSHORTCUT->resource->resource()) {
            PSHORTCUT->resource->noMemory();
            shortcuts.pop_back();
            return;
        }

        PSHORTCUT->resource->setDestroy([this](CHyprlandGlobalShortcutV1* pMgr) { std::erase_if(shortcuts, [&](const auto& other) { return other->resource.get() == pMgr; }); });
    });
}

bool CShortcutClient::good() {
    return resource->resource();
}

CGlobalShortcutsProtocol::CGlobalShortcutsProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CGlobalShortcutsProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESROUCE = m_vClients.emplace_back(makeShared<CShortcutClient>(makeShared<CHyprlandGlobalShortcutsManagerV1>(client, ver, id)));

    if (!RESROUCE->good()) {
        wl_client_post_no_memory(client);
        m_vClients.pop_back();
        return;
    }
}

void CGlobalShortcutsProtocol::destroyResource(CShortcutClient* client) {
    std::erase_if(m_vClients, [&](const auto& other) { return other.get() == client; });
}

bool CGlobalShortcutsProtocol::isTaken(std::string appid, std::string trigger) {
    for (auto& c : m_vClients) {
        for (auto& sh : c->shortcuts) {
            if (sh->appid == appid && sh->id == trigger) {
                return true;
            }
        }
    }

    return false;
}

void CGlobalShortcutsProtocol::sendGlobalShortcutEvent(std::string appid, std::string trigger, bool pressed) {
    for (auto& c : m_vClients) {
        for (auto& sh : c->shortcuts) {
            if (sh->appid == appid && sh->id == trigger) {
                timespec now;
                clock_gettime(CLOCK_MONOTONIC, &now);
                uint32_t tvSecHi = (sizeof(now.tv_sec) > 4) ? now.tv_sec >> 32 : 0;
                uint32_t tvSecLo = now.tv_sec & 0xFFFFFFFF;
                if (pressed)
                    sh->resource->sendPressed(tvSecHi, tvSecLo, now.tv_nsec);
                else
                    sh->resource->sendReleased(tvSecHi, tvSecLo, now.tv_nsec);
            }
        }
    }
}

std::vector<SShortcut> CGlobalShortcutsProtocol::getAllShortcuts() {
    std::vector<SShortcut> copy;
    for (auto& c : m_vClients) {
        for (auto& sh : c->shortcuts) {
            copy.push_back(*sh);
        }
    }

    return copy;
}
