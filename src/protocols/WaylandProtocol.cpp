#include "WaylandProtocol.hpp"
#include "../Compositor.hpp"

static void bindManagerInternal(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    ((IWaylandProtocol*)data)->bindManager(client, data, ver, id);
}

static void displayDestroyInternal(struct wl_listener* listener, void* data) {
    IWaylandProtocolDestroyWrapper* wrap  = wl_container_of(listener, wrap, listener);
    IWaylandProtocol*               proto = wrap->parent;
    proto->onDisplayDestroy();
}

void IWaylandProtocol::onDisplayDestroy() {
    wl_list_remove(&m_liDisplayDestroy.listener.link);
    wl_list_init(&m_liDisplayDestroy.listener.link);
    wl_global_destroy(m_pGlobal);
}

IWaylandProtocol::IWaylandProtocol(const wl_interface* iface, const int& ver, const std::string& name) : m_szName(name) {
    m_pGlobal = wl_global_create(g_pCompositor->m_sWLDisplay, iface, ver, this, &bindManagerInternal);

    if (!m_pGlobal) {
        LOGM(ERR, "could not create a global [{}]", m_szName);
        return;
    }

    wl_list_init(&m_liDisplayDestroy.listener.link);
    m_liDisplayDestroy.listener.notify = displayDestroyInternal;
    m_liDisplayDestroy.parent          = this;
    wl_display_add_destroy_listener(g_pCompositor->m_sWLDisplay, &m_liDisplayDestroy.listener);

    LOGM(LOG, "Registered global [{}]", m_szName);
}

IWaylandProtocol::~IWaylandProtocol() {
    onDisplayDestroy();
}

void IWaylandProtocol::removeGlobal() {
    wl_global_remove(m_pGlobal);
}
