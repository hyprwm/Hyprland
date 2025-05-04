#include "WaylandProtocol.hpp"
#include "../Compositor.hpp"

static void bindManagerInternal(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    ((IWaylandProtocol*)data)->bindManager(client, data, ver, id);
}

static void displayDestroyInternal(struct wl_listener* listener, void* data) {
    SIWaylandProtocolDestroyWrapper* wrap  = wl_container_of(listener, wrap, listener);
    IWaylandProtocol*                proto = wrap->parent;
    proto->onDisplayDestroy();
}

void IWaylandProtocol::onDisplayDestroy() {
    wl_list_remove(&m_liDisplayDestroy.listener.link);
    wl_list_init(&m_liDisplayDestroy.listener.link);
    if (m_global) {
        wl_global_destroy(m_global);
        m_global = nullptr;
    }
}

IWaylandProtocol::IWaylandProtocol(const wl_interface* iface, const int& ver, const std::string& name) :
    m_name(name), m_global(wl_global_create(g_pCompositor->m_wlDisplay, iface, ver, this, &bindManagerInternal)) {

    if UNLIKELY (!m_global) {
        LOGM(ERR, "could not create a global [{}]", m_name);
        return;
    }

    wl_list_init(&m_liDisplayDestroy.listener.link);
    m_liDisplayDestroy.listener.notify = displayDestroyInternal;
    m_liDisplayDestroy.parent          = this;
    wl_display_add_destroy_listener(g_pCompositor->m_wlDisplay, &m_liDisplayDestroy.listener);

    LOGM(LOG, "Registered global [{}]", m_name);
}

IWaylandProtocol::~IWaylandProtocol() {
    onDisplayDestroy();
}

void IWaylandProtocol::removeGlobal() {
    if (m_global)
        wl_global_remove(m_global);
}

wl_global* IWaylandProtocol::getGlobal() {
    return m_global;
}
