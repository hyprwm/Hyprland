#include "SecurityContext.hpp"
#include "../Compositor.hpp"
#include <sys/socket.h>
using namespace Hyprutils::OS;

static int onListenFdEvent(int fd, uint32_t mask, void* data) {
    auto sc = (CSecurityContext*)data;
    sc->onListen(mask);
    return 0;
}

static int onCloseFdEvent(int fd, uint32_t mask, void* data) {
    auto sc = (CSecurityContext*)data;
    sc->onClose(mask);
    return 0;
}

SP<CSecurityContextSandboxedClient> CSecurityContextSandboxedClient::create(CFileDescriptor clientFD_) {
    auto p = SP<CSecurityContextSandboxedClient>(new CSecurityContextSandboxedClient(std::move(clientFD_)));
    if (!p->m_client)
        return nullptr;
    return p;
}

static void onSecurityContextClientDestroy(wl_listener* l, void* d) {
    SCSecurityContextSandboxedClientDestroyWrapper* wrap   = wl_container_of(l, wrap, listener);
    CSecurityContextSandboxedClient*                client = wrap->parent;
    client->onDestroy();
}

CSecurityContextSandboxedClient::CSecurityContextSandboxedClient(CFileDescriptor clientFD_) : m_clientFD(std::move(clientFD_)) {
    m_client = wl_client_create(g_pCompositor->m_wlDisplay, m_clientFD.get());
    if (!m_client)
        return;

    wl_list_init(&m_destroyListener.listener.link);
    m_destroyListener.listener.notify = ::onSecurityContextClientDestroy;
    m_destroyListener.parent          = this;
    wl_client_add_destroy_late_listener(m_client, &m_destroyListener.listener);
}

CSecurityContextSandboxedClient::~CSecurityContextSandboxedClient() {
    wl_list_remove(&m_destroyListener.listener.link);
    wl_list_init(&m_destroyListener.listener.link);
}

void CSecurityContextSandboxedClient::onDestroy() {
    std::erase_if(PROTO::securityContext->m_sandboxedClients, [this](const auto& e) { return e.get() == this; });
}

CSecurityContext::CSecurityContext(SP<CWpSecurityContextV1> resource_, int listenFD_, int closeFD_) : m_listenFD(listenFD_), m_closeFD(closeFD_), m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setDestroy([this](CWpSecurityContextV1* r) {
        LOGM(LOG, "security_context at 0x{:x}: resource destroyed, keeping context until fd hangup", (uintptr_t)this);
        m_resource = nullptr;
    });
    m_resource->setOnDestroy([this](CWpSecurityContextV1* r) {
        LOGM(LOG, "security_context at 0x{:x}: resource destroyed, keeping context until fd hangup", (uintptr_t)this);
        m_resource = nullptr;
    });

    LOGM(LOG, "New security_context at 0x{:x}", (uintptr_t)this);

    m_resource->setSetSandboxEngine([this](CWpSecurityContextV1* r, const char* engine) {
        if UNLIKELY (!m_sandboxEngine.empty()) {
            r->error(WP_SECURITY_CONTEXT_V1_ERROR_ALREADY_SET, "Sandbox engine already set");
            return;
        }

        if UNLIKELY (m_committed) {
            r->error(WP_SECURITY_CONTEXT_V1_ERROR_ALREADY_USED, "Context already committed");
            return;
        }

        m_sandboxEngine = engine ? engine : "(null)";
        LOGM(LOG, "security_context at 0x{:x} sets engine to {}", (uintptr_t)this, m_sandboxEngine);
    });

    m_resource->setSetAppId([this](CWpSecurityContextV1* r, const char* appid) {
        if UNLIKELY (!m_appID.empty()) {
            r->error(WP_SECURITY_CONTEXT_V1_ERROR_ALREADY_SET, "Sandbox appid already set");
            return;
        }

        if UNLIKELY (m_committed) {
            r->error(WP_SECURITY_CONTEXT_V1_ERROR_ALREADY_USED, "Context already committed");
            return;
        }

        m_appID = appid ? appid : "(null)";
        LOGM(LOG, "security_context at 0x{:x} sets appid to {}", (uintptr_t)this, m_appID);
    });

    m_resource->setSetInstanceId([this](CWpSecurityContextV1* r, const char* instance) {
        if UNLIKELY (!m_instanceID.empty()) {
            r->error(WP_SECURITY_CONTEXT_V1_ERROR_ALREADY_SET, "Sandbox instance already set");
            return;
        }

        if UNLIKELY (m_committed) {
            r->error(WP_SECURITY_CONTEXT_V1_ERROR_ALREADY_USED, "Context already committed");
            return;
        }

        m_instanceID = instance ? instance : "(null)";
        LOGM(LOG, "security_context at 0x{:x} sets instance to {}", (uintptr_t)this, m_instanceID);
    });

    m_resource->setCommit([this](CWpSecurityContextV1* r) {
        m_committed = true;

        LOGM(LOG, "security_context at 0x{:x} commits", (uintptr_t)this);

        m_listenSource = wl_event_loop_add_fd(g_pCompositor->m_wlEventLoop, m_listenFD.get(), WL_EVENT_READABLE, ::onListenFdEvent, this);
        m_closeSource  = wl_event_loop_add_fd(g_pCompositor->m_wlEventLoop, m_closeFD.get(), 0, ::onCloseFdEvent, this);

        if (!m_listenSource || !m_closeSource) {
            r->noMemory();
            return;
        }
    });
}

CSecurityContext::~CSecurityContext() {
    if (m_listenSource)
        wl_event_source_remove(m_listenSource);
    if (m_closeSource)
        wl_event_source_remove(m_closeSource);
}

bool CSecurityContext::good() {
    return m_resource->resource();
}

void CSecurityContext::onListen(uint32_t mask) {
    if UNLIKELY (mask & (WL_EVENT_HANGUP | WL_EVENT_ERROR)) {
        LOGM(ERR, "security_context at 0x{:x} got an error in listen", (uintptr_t)this);
        PROTO::securityContext->destroyContext(this);
        return;
    }

    if (!(mask & WL_EVENT_READABLE))
        return;

    CFileDescriptor clientFD{accept(m_listenFD.get(), nullptr, nullptr)};
    if UNLIKELY (!clientFD.isValid()) {
        LOGM(ERR, "security_context at 0x{:x} couldn't accept", (uintptr_t)this);
        return;
    }

    auto newClient = CSecurityContextSandboxedClient::create(std::move(clientFD));
    if UNLIKELY (!newClient) {
        LOGM(ERR, "security_context at 0x{:x} couldn't create a client", (uintptr_t)this);
        return;
    }

    PROTO::securityContext->m_sandboxedClients.emplace_back(newClient);

    LOGM(LOG, "security_context at 0x{:x} got a new wl_client 0x{:x}", (uintptr_t)this, (uintptr_t)newClient->m_client);
}

void CSecurityContext::onClose(uint32_t mask) {
    if (!(mask & (WL_EVENT_ERROR | WL_EVENT_HANGUP)))
        return;

    PROTO::securityContext->destroyContext(this);
}

CSecurityContextManagerResource::CSecurityContextManagerResource(SP<CWpSecurityContextManagerV1> resource_) : m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setDestroy([this](CWpSecurityContextManagerV1* r) { PROTO::securityContext->destroyResource(this); });
    m_resource->setOnDestroy([this](CWpSecurityContextManagerV1* r) { PROTO::securityContext->destroyResource(this); });

    m_resource->setCreateListener([](CWpSecurityContextManagerV1* r, uint32_t id, int32_t lfd, int32_t cfd) {
        const auto RESOURCE =
            PROTO::securityContext->m_contexts.emplace_back(makeShared<CSecurityContext>(makeShared<CWpSecurityContextV1>(r->client(), r->version(), id), lfd, cfd));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::securityContext->m_contexts.pop_back();
            return;
        }
    });
}

bool CSecurityContextManagerResource::good() {
    return m_resource->resource();
}

CSecurityContextProtocol::CSecurityContextProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CSecurityContextProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(makeShared<CSecurityContextManagerResource>(makeShared<CWpSecurityContextManagerV1>(client, ver, id)));

    if UNLIKELY (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_managers.pop_back();
        return;
    }
}

void CSecurityContextProtocol::destroyResource(CSecurityContextManagerResource* res) {
    std::erase_if(m_managers, [&](const auto& other) { return other.get() == res; });
}

void CSecurityContextProtocol::destroyContext(CSecurityContext* context) {
    std::erase_if(m_contexts, [&](const auto& other) { return other.get() == context; });
}

bool CSecurityContextProtocol::isClientSandboxed(const wl_client* client) {
    return std::find_if(m_sandboxedClients.begin(), m_sandboxedClients.end(), [client](const auto& e) { return e->m_client == client; }) != m_sandboxedClients.end();
}
