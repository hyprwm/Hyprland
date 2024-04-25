#include "ShortcutsInhibit.hpp"
#include <algorithm>
#include "../Compositor.hpp"

#define LOGM PROTO::shortcutsInhibit->protoLog

CKeyboardShortcutsInhibitor::CKeyboardShortcutsInhibitor(SP<CZwpKeyboardShortcutsInhibitorV1> resource_, wlr_surface* surf) : resource(resource_), pSurface(surf) {
    if (!resource->resource())
        return;

    resource->setDestroy([this](CZwpKeyboardShortcutsInhibitorV1* pMgr) { PROTO::shortcutsInhibit->destroyInhibitor(this); });
    resource->setOnDestroy([this](CZwpKeyboardShortcutsInhibitorV1* pMgr) { PROTO::shortcutsInhibit->destroyInhibitor(this); });

    // I don't really care about following the spec here that much,
    // let's make the app believe it's always active
    resource->sendActive();
}

wlr_surface* CKeyboardShortcutsInhibitor::surface() {
    return pSurface;
}

bool CKeyboardShortcutsInhibitor::good() {
    return resource->resource();
}

CKeyboardShortcutsInhibitProtocol::CKeyboardShortcutsInhibitProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CKeyboardShortcutsInhibitProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(std::make_unique<CZwpKeyboardShortcutsInhibitManagerV1>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CZwpKeyboardShortcutsInhibitManagerV1* p) { this->onManagerResourceDestroy(p->resource()); });

    RESOURCE->setDestroy([this](CZwpKeyboardShortcutsInhibitManagerV1* pMgr) { this->onManagerResourceDestroy(pMgr->resource()); });
    RESOURCE->setInhibitShortcuts(
        [this](CZwpKeyboardShortcutsInhibitManagerV1* pMgr, uint32_t id, wl_resource* surface, wl_resource* seat) { this->onInhibit(pMgr, id, surface, seat); });
}

void CKeyboardShortcutsInhibitProtocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other->resource() == res; });
}

void CKeyboardShortcutsInhibitProtocol::destroyInhibitor(CKeyboardShortcutsInhibitor* inhibitor) {
    std::erase_if(m_vInhibitors, [&](const auto& other) { return other.get() == inhibitor; });
}

void CKeyboardShortcutsInhibitProtocol::onInhibit(CZwpKeyboardShortcutsInhibitManagerV1* pMgr, uint32_t id, wl_resource* surface, wl_resource* seat) {
    wlr_surface* surf   = wlr_surface_from_resource(surface);
    const auto   CLIENT = wl_resource_get_client(pMgr->resource());

    for (auto& in : m_vInhibitors) {
        if (in->surface() != surf)
            continue;

        wl_resource_post_error(pMgr->resource(), ZWP_KEYBOARD_SHORTCUTS_INHIBIT_MANAGER_V1_ERROR_ALREADY_INHIBITED, "Already inhibited for surface resource");
        return;
    }

    const auto RESOURCE = m_vInhibitors
                              .emplace_back(std::make_unique<CKeyboardShortcutsInhibitor>(
                                  std::make_shared<CZwpKeyboardShortcutsInhibitorV1>(CLIENT, wl_resource_get_version(pMgr->resource()), id), surf))
                              .get();

    if (!RESOURCE->good()) {
        wl_resource_post_no_memory(pMgr->resource());
        m_vInhibitors.pop_back();
        LOGM(ERR, "Failed to create an inhibitor resource");
        return;
    }
}

bool CKeyboardShortcutsInhibitProtocol::isInhibited() {
    if (!g_pCompositor->m_pLastFocus)
        return false;

    for (auto& in : m_vInhibitors) {
        if (in->surface() != g_pCompositor->m_pLastFocus)
            continue;

        return true;
    }

    return false;
}
