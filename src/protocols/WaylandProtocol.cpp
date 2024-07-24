#include "WaylandProtocol.hpp"
#include "../Compositor.hpp"

static void bindManagerInternal(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    ((IWaylandProtocol*)data)->bindManager(client, data, ver, id);
}

static void displayDestroyInternal(struct wl_listener* listener, void* data) {
    IWaylandProtocol* proto = wl_container_of(listener, proto, m_liDisplayDestroy);
    proto->onDisplayDestroy();
}

void IWaylandProtocol::onDisplayDestroy() {
    wl_list_remove(&m_liDisplayDestroy.link);
    wl_list_init(&m_liDisplayDestroy.link);
    wl_global_destroy(m_pGlobal);
}

IWaylandProtocol::IWaylandProtocol(const wl_interface* iface, const int& ver, const std::string& name) : m_szName(name) {
    m_pGlobal = wl_global_create(g_pCompositor->m_sWLDisplay, iface, ver, this, &bindManagerInternal);

    if (!m_pGlobal) {
        protoLog(ERR, "could not create a global");
        return;
    }

    m_liDisplayDestroy.notify = displayDestroyInternal;
    wl_display_add_destroy_listener(g_pCompositor->m_sWLDisplay, &m_liDisplayDestroy);

    protoLog(LOG, "Registered global");
}

IWaylandProtocol::~IWaylandProtocol() {
    onDisplayDestroy();
}

void IWaylandProtocol::removeGlobal() {
    wl_global_remove(m_pGlobal);
}
