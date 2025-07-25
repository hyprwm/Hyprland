#include "GlobalShortcuts.hpp"
#include "../helpers/time/Time.hpp"

CShortcutClient::CShortcutClient(SP<CHyprlandGlobalShortcutsManagerV1> resource_) : m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setOnDestroy([this](CHyprlandGlobalShortcutsManagerV1* pMgr) { PROTO::globalShortcuts->destroyResource(this); });
    m_resource->setDestroy([this](CHyprlandGlobalShortcutsManagerV1* pMgr) { PROTO::globalShortcuts->destroyResource(this); });

    m_resource->setRegisterShortcut([this](CHyprlandGlobalShortcutsManagerV1* pMgr, uint32_t shortcut, const char* id, const char* app_id, const char* description,
                                           const char* trigger_description) {
        if UNLIKELY (PROTO::globalShortcuts->isTaken(id, app_id)) {
            m_resource->error(HYPRLAND_GLOBAL_SHORTCUTS_MANAGER_V1_ERROR_ALREADY_TAKEN, "Combination is taken");
            return;
        }

        const auto PSHORTCUT   = m_shortcuts.emplace_back(makeShared<SShortcut>(makeShared<CHyprlandGlobalShortcutV1>(m_resource->client(), m_resource->version(), shortcut)));
        PSHORTCUT->id          = id;
        PSHORTCUT->description = description;
        PSHORTCUT->appid       = app_id;
        PSHORTCUT->shortcut    = shortcut;

        if UNLIKELY (!PSHORTCUT->resource->resource()) {
            PSHORTCUT->resource->noMemory();
            m_shortcuts.pop_back();
            return;
        }

        PSHORTCUT->resource->setDestroy([this](CHyprlandGlobalShortcutV1* pMgr) { std::erase_if(m_shortcuts, [&](const auto& other) { return other->resource.get() == pMgr; }); });
    });
}

bool CShortcutClient::good() {
    return m_resource->resource();
}

CGlobalShortcutsProtocol::CGlobalShortcutsProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CGlobalShortcutsProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_clients.emplace_back(makeShared<CShortcutClient>(makeShared<CHyprlandGlobalShortcutsManagerV1>(client, ver, id)));

    if UNLIKELY (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_clients.pop_back();
        return;
    }
}

void CGlobalShortcutsProtocol::destroyResource(CShortcutClient* client) {
    std::erase_if(m_clients, [&](const auto& other) { return other.get() == client; });
}

bool CGlobalShortcutsProtocol::isTaken(std::string appid, std::string trigger) {
    for (auto const& c : m_clients) {
        for (auto const& sh : c->m_shortcuts) {
            if (sh->appid == appid && sh->id == trigger) {
                return true;
            }
        }
    }

    return false;
}

void CGlobalShortcutsProtocol::sendGlobalShortcutEvent(std::string appid, std::string trigger, bool pressed) {
    for (auto const& c : m_clients) {
        for (auto const& sh : c->m_shortcuts) {
            if (sh->appid == appid && sh->id == trigger) {
                const auto [sec, nsec] = Time::secNsec(Time::steadyNow());
                uint32_t tvSecHi       = (sizeof(sec) > 4) ? sec >> 32 : 0;
                uint32_t tvSecLo       = sec & 0xFFFFFFFF;
                if (pressed)
                    sh->resource->sendPressed(tvSecHi, tvSecLo, nsec);
                else
                    sh->resource->sendReleased(tvSecHi, tvSecLo, nsec);
            }
        }
    }
}

std::vector<SShortcut> CGlobalShortcutsProtocol::getAllShortcuts() {
    std::vector<SShortcut> copy;
    // calculate the total number of shortcuts, precomputing size is linear
    // and potential reallocation is more costly then the added precompute overhead of looping
    // and finding the total size.
    size_t totalShortcuts = 0;
    for (const auto& c : m_clients) {
        totalShortcuts += c->m_shortcuts.size();
    }

    // reserve number of elements to avoid reallocations
    copy.reserve(totalShortcuts);

    for (const auto& c : m_clients) {
        for (const auto& sh : c->m_shortcuts) {
            copy.push_back(*sh);
        }
    }

    return copy;
}
