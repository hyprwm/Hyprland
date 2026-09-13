#include "WaylandProtocol.hpp"
#include "../Compositor.hpp"

static void bindManagerInternal(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    sc<IWaylandProtocol*>(data)->bindManager(client, data, ver, id);
}

static void displayDestroyInternal(struct wl_listener* listener, void* data) {
    SIWaylandProtocolDestroyWrapper* wrap  = wl_container_of(listener, wrap, listener);
    IWaylandProtocol*                proto = wrap->parent;
    proto->onDisplayDestroy();
}

void IWaylandProtocol::onDisplayDestroy() {
    if (m_displayDestroyed)
        return;
    m_displayDestroyed = true;

    wl_list_remove(&m_liDisplayDestroy.listener.link);
    wl_list_init(&m_liDisplayDestroy.listener.link);
    if (m_global) {
        wl_global_destroy(m_global);
        m_global = nullptr;
    }
}

IWaylandProtocol::IWaylandProtocol(const wl_interface* iface, const int& ver, const std::string& name) :
    m_name(name), m_global(wl_global_create(g_pCompositor->m_wlDisplay, iface, ver, this, &bindManagerInternal)) {

    // wl_list_init() must run unconditionally, even if wl_global_create() failed
    // below (which can happen legitimately, e.g. a runtime wayland-protocols
    // version older than what this interface was requested at). Without this,
    // m_liDisplayDestroy.listener.link is left as uninitialized memory on that
    // path, and onDisplayDestroy()'s wl_list_remove() on it is a segfault on
    // whatever garbage happens to be there -- deterministically, on every
    // graceful exit, for any protocol whose global creation ever fails this way.
    wl_list_init(&m_liDisplayDestroy.listener.link);

    if UNLIKELY (!m_global) {
        LOG(Log::ERR, "could not create a global [{}]", m_name);
        return;
    }

    m_liDisplayDestroy.listener.notify = displayDestroyInternal;
    m_liDisplayDestroy.parent          = this;
    wl_display_add_destroy_listener(g_pCompositor->m_wlDisplay, &m_liDisplayDestroy.listener);

    LOG(Log::DEBUG, "Registered global [{}]", m_name);
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
