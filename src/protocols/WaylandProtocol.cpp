#include "WaylandProtocol.hpp"
#include "../Compositor.hpp"

static void resourceDestroyNotify(wl_listener* listener, void* data) {
    CWaylandResource* pResource = wl_container_of(listener, pResource, m_liResourceDestroy);
    pResource->onResourceDestroy();
}

CWaylandResource::CWaylandResource(wl_client* client, const wl_interface* wlInterface, uint32_t version, uint32_t id, bool destroyInDestructor) {
    m_pWLResource = wl_resource_create(client, wlInterface, version, id);

    if (!m_pWLResource) {
        wl_client_post_no_memory(client);
        return;
    }

    m_pWLClient            = client;
    m_bDestroyInDestructor = destroyInDestructor;

    wl_list_init(&m_liResourceDestroy.link);
    m_liResourceDestroy.notify = resourceDestroyNotify;
    wl_resource_add_destroy_listener(m_pWLResource, &m_liResourceDestroy);

    Debug::log(TRACE, "[wl res %lx] created", m_pWLResource);
}

void CWaylandResource::onResourceDestroy() {
    Debug::log(TRACE, "[wl res %lx] now defunct", m_pWLResource);
    m_bDefunct = true;
}

void CWaylandResource::blockDestroy(bool block) {
    m_bDestroyInDestructor = !block;
}

CWaylandResource::~CWaylandResource() {
    const bool DESTROY = m_pWLResource && m_bDestroyInDestructor && !m_bDefunct;

    if (good()) {
        wl_list_remove(&m_liResourceDestroy.link);
        wl_list_init(&m_liResourceDestroy.link);
    }

    Debug::log(TRACE, "[wl res %lx] destroying (wl_resource_destroy will be %s)", m_pWLResource, (DESTROY ? "sent" : "not sent"));

    if (DESTROY)
        wl_resource_destroy(m_pWLResource);
}

bool CWaylandResource::good() {
    return m_pWLResource && !m_bDefunct;
}

wl_resource* CWaylandResource::resource() {
    RASSERT(good(), "Attempted to call resource() on a bad resource");

    return m_pWLResource;
}

uint32_t CWaylandResource::version() {
    RASSERT(good(), "Attempted to call version() on a bad resource");

    return wl_resource_get_version(m_pWLResource);
}

void CWaylandResource::setImplementation(const void* impl, void* data, wl_resource_destroy_func_t df) {
    RASSERT(good(), "Attempted to call setImplementation() on a bad resource");
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
