#include "WaylandProtocol.hpp"
#include "../Compositor.hpp"

CWaylandResource::CWaylandResource(wl_client* client, const wl_interface* wlInterface, uint32_t version, uint32_t id, bool destroyInDestructor) {
    m_pWLResource = wl_resource_create(client, wlInterface, version, id);

    if (!m_pWLResource) {
        wl_client_post_no_memory(client);
        return;
    }

    m_pWLClient            = client;
    m_bDestroyInDestructor = destroyInDestructor;

    Debug::log(LOG, "[wl res %lx] created", m_pWLResource);
}

CWaylandResource::~CWaylandResource() {
    if (m_pWLResource && m_bDestroyInDestructor)
        wl_resource_destroy(m_pWLResource);

    Debug::log(LOG, "[wl res %lx] destroyed (wl_resource_destroy %s)", m_pWLResource, (m_pWLResource && m_bDestroyInDestructor ? "sent" : "not sent"));
}

bool CWaylandResource::good() {
    return resource();
}

wl_resource* CWaylandResource::resource() {
    return m_pWLResource;
}

uint32_t CWaylandResource::version() {
    return wl_resource_get_version(m_pWLResource);
}

void CWaylandResource::setImplementation(const void* impl, void* data, wl_resource_destroy_func_t df) {
    RASSERT(!m_bImplementationSet, "Wayland Resource %lx already has an implementation, cannot re-set!", m_pWLResource);

    wl_resource_set_implementation(m_pWLResource, impl, data, df);

    Debug::log(LOG, "[wl res %lx] set impl to %lx", m_pWLResource, impl);

    m_bImplementationSet = true;
}

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
        Debug::log(ERR, "[proto %s] could not create a global", name.c_str());
        return;
    }

    m_liDisplayDestroy.notify = displayDestroyInternal;
    wl_display_add_destroy_listener(g_pCompositor->m_sWLDisplay, &m_liDisplayDestroy);

    Debug::log(LOG, "[proto %s] started", name.c_str());
}

IWaylandProtocol::~IWaylandProtocol() {
    onDisplayDestroy();
}
