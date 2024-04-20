#include "WaylandProtocol.hpp"
#include "../Compositor.hpp"

static void bindManagerInternal(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    ((IWaylandProtocol*)data)->bindManager(client, data, ver, id);
}

static void displayDestroyInternal(struct wl_listener* listener, void* data) {
    ((IWaylandProtocol*)data)->onDisplayDestroy();
}

void IWaylandProtocol::onDisplayDestroy() {
    wl_global_destroy(m_pGlobal);
}

IWaylandProtocol::IWaylandProtocol(const wl_interface* iface, const int& ver, const std::string& name) {
    m_pGlobal = wl_global_create(g_pCompositor->m_sWLDisplay, iface, ver, this, &bindManagerInternal);

    if (!m_pGlobal) {
        Debug::log(ERR, "[proto {}] could not create a global", name);
        return;
    }

    m_liDisplayDestroy.notify = displayDestroyInternal;
    wl_display_add_destroy_listener(g_pCompositor->m_sWLDisplay, &m_liDisplayDestroy);

    Debug::log(LOG, "[proto {}] started", name);
}

IWaylandProtocol::~IWaylandProtocol() {
    onDisplayDestroy();
}
