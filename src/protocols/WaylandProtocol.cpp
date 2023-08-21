#include "WaylandProtocol.hpp"
#include "../Compositor.hpp"

static void resourceDestroyNotify(wl_listener* listener, void* data) {
    CWaylandResource* pResource = wl_container_of(listener, pResource, m_liResourceDestroy);
    pResource->markDefunct();
}

CWaylandResource::CWaylandResource(wl_client* client, const wl_interface* wlInterface, uint32_t version, uint32_t id) {
    m_pWLResource = wl_resource_create(client, wlInterface, version, id);

    if (!m_pWLResource) {
        wl_client_post_no_memory(client);
        return;
    }

    wl_resource_set_user_data(m_pWLResource, this);

    m_pWLClient = client;

    wl_list_init(&m_liResourceDestroy.link);
    m_liResourceDestroy.notify = resourceDestroyNotify;
    wl_resource_add_destroy_listener(m_pWLResource, &m_liResourceDestroy);

    Debug::log(TRACE, "[wl res %lx] created", m_pWLResource);
}

void CWaylandResource::markDefunct() {
    if (m_bDefunct)
        return;

    Debug::log(TRACE, "[wl res %lx] now defunct", m_pWLResource);
    m_bDefunct = true;
}

CWaylandResource::~CWaylandResource() {
    const bool DESTROY = m_pWLResource && !m_bDefunct;

    wl_list_remove(&m_liResourceDestroy.link);
    wl_list_init(&m_liResourceDestroy.link);

    if (m_pWLResource)
        wl_resource_set_user_data(m_pWLResource, nullptr);

    Debug::log(TRACE, "[wl res %lx] destroying (wl_resource_destroy will be %s)", m_pWLResource, (DESTROY ? "sent" : "not sent"));

    if (DESTROY)
        wl_resource_destroy(m_pWLResource);
}

bool CWaylandResource::good() {
    return m_pWLResource && !m_bDefunct;
}

wl_resource* CWaylandResource::resource() {
    return m_pWLResource;
}

uint32_t CWaylandResource::version() {
    RASSERT(good(), "Attempted to call version() on a bad resource");

    return wl_resource_get_version(m_pWLResource);
}

void CWaylandResource::setImplementation(const void* impl, wl_resource_destroy_func_t df) {
    RASSERT(good(), "Attempted to call setImplementation() on a bad resource");
    RASSERT(!m_bImplementationSet, "Wayland Resource %lx already has an implementation, cannot re-set!", m_pWLResource);

    wl_resource_set_implementation(m_pWLResource, impl, this, df);

    Debug::log(TRACE, "[wl res %lx] set impl to %lx", m_pWLResource, impl);

    m_bImplementationSet = true;
}

void CWaylandResource::setData(void* data) {
    Debug::log(TRACE, "[wl res %lx] set data to %lx", m_pWLResource, data);
    m_pData = data;
}

void* CWaylandResource::data() {
    return m_pData;
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
