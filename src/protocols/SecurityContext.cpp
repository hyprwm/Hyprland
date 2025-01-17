#include "SecurityContext.hpp"
#include "../Compositor.hpp"
#include <sys/socket.h>

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

SP<CSecurityContextSandboxedClient> CSecurityContextSandboxedClient::create(int clientFD_) {
    auto p = SP<CSecurityContextSandboxedClient>(new CSecurityContextSandboxedClient(clientFD_));
    if (!p->client)
        return nullptr;
    return p;
}

static void onSecurityContextClientDestroy(wl_listener* l, void* d) {
    SCSecurityContextSandboxedClientDestroyWrapper* wrap   = wl_container_of(l, wrap, listener);
    CSecurityContextSandboxedClient*                client = wrap->parent;
    client->onDestroy();
}

CSecurityContextSandboxedClient::CSecurityContextSandboxedClient(int clientFD_) : clientFD(clientFD_) {
    client = wl_client_create(g_pCompositor->m_sWLDisplay, clientFD);
    if (!client)
        return;

    wl_list_init(&destroyListener.listener.link);
    destroyListener.listener.notify = ::onSecurityContextClientDestroy;
    destroyListener.parent          = this;
    wl_client_add_destroy_late_listener(client, &destroyListener.listener);
}

CSecurityContextSandboxedClient::~CSecurityContextSandboxedClient() {
    wl_list_remove(&destroyListener.listener.link);
    wl_list_init(&destroyListener.listener.link);
    close(clientFD);
}

void CSecurityContextSandboxedClient::onDestroy() {
    std::erase_if(PROTO::securityContext->m_vSandboxedClients, [this](const auto& e) { return e.get() == this; });
}

CSecurityContext::CSecurityContext(SP<CWpSecurityContextV1> resource_, int listenFD_, int closeFD_) : listenFD(listenFD_), closeFD(closeFD_), resource(resource_) {
    if UNLIKELY (!good())
        return;

    resource->setDestroy([this](CWpSecurityContextV1* r) {
        LOGM(LOG, "security_context at 0x{:x}: resource destroyed, keeping context until fd hangup", (uintptr_t)this);
        resource = nullptr;
    });
    resource->setOnDestroy([this](CWpSecurityContextV1* r) {
        LOGM(LOG, "security_context at 0x{:x}: resource destroyed, keeping context until fd hangup", (uintptr_t)this);
        resource = nullptr;
    });

    LOGM(LOG, "New security_context at 0x{:x}", (uintptr_t)this);

    resource->setSetSandboxEngine([this](CWpSecurityContextV1* r, const char* engine) {
        if UNLIKELY (!sandboxEngine.empty()) {
            r->error(WP_SECURITY_CONTEXT_V1_ERROR_ALREADY_SET, "Sandbox engine already set");
            return;
        }

        if UNLIKELY (committed) {
            r->error(WP_SECURITY_CONTEXT_V1_ERROR_ALREADY_USED, "Context already committed");
            return;
        }

        sandboxEngine = engine ? engine : "(null)";
        LOGM(LOG, "security_context at 0x{:x} sets engine to {}", (uintptr_t)this, sandboxEngine);
    });

    resource->setSetAppId([this](CWpSecurityContextV1* r, const char* appid) {
        if UNLIKELY (!appID.empty()) {
            r->error(WP_SECURITY_CONTEXT_V1_ERROR_ALREADY_SET, "Sandbox appid already set");
            return;
        }

        if UNLIKELY (committed) {
            r->error(WP_SECURITY_CONTEXT_V1_ERROR_ALREADY_USED, "Context already committed");
            return;
        }

        appID = appid ? appid : "(null)";
        LOGM(LOG, "security_context at 0x{:x} sets appid to {}", (uintptr_t)this, appID);
    });

    resource->setSetInstanceId([this](CWpSecurityContextV1* r, const char* instance) {
        if UNLIKELY (!instanceID.empty()) {
            r->error(WP_SECURITY_CONTEXT_V1_ERROR_ALREADY_SET, "Sandbox instance already set");
            return;
        }

        if UNLIKELY (committed) {
            r->error(WP_SECURITY_CONTEXT_V1_ERROR_ALREADY_USED, "Context already committed");
            return;
        }

        instanceID = instance ? instance : "(null)";
        LOGM(LOG, "security_context at 0x{:x} sets instance to {}", (uintptr_t)this, instanceID);
    });

    resource->setCommit([this](CWpSecurityContextV1* r) {
        committed = true;

        LOGM(LOG, "security_context at 0x{:x} commits", (uintptr_t)this);

        listenSource = wl_event_loop_add_fd(g_pCompositor->m_sWLEventLoop, listenFD, WL_EVENT_READABLE, ::onListenFdEvent, this);
        closeSource  = wl_event_loop_add_fd(g_pCompositor->m_sWLEventLoop, closeFD, 0, ::onCloseFdEvent, this);

        if (!listenSource || !closeSource) {
            r->noMemory();
            return;
        }
    });
}

CSecurityContext::~CSecurityContext() {
    if (listenSource)
        wl_event_source_remove(listenSource);
    if (closeSource)
        wl_event_source_remove(closeSource);
}

bool CSecurityContext::good() {
    return resource->resource();
}

void CSecurityContext::onListen(uint32_t mask) {
    if UNLIKELY (mask & (WL_EVENT_HANGUP | WL_EVENT_ERROR)) {
        LOGM(ERR, "security_context at 0x{:x} got an error in listen", (uintptr_t)this);
        PROTO::securityContext->destroyContext(this);
        return;
    }

    if (!(mask & WL_EVENT_READABLE))
        return;

    int clientFD = accept(listenFD, nullptr, nullptr);
    if UNLIKELY (clientFD < 0) {
        LOGM(ERR, "security_context at 0x{:x} couldn't accept", (uintptr_t)this);
        return;
    }

    auto newClient = CSecurityContextSandboxedClient::create(clientFD);
    if UNLIKELY (!newClient) {
        LOGM(ERR, "security_context at 0x{:x} couldn't create a client", (uintptr_t)this);
        close(clientFD);
        return;
    }

    PROTO::securityContext->m_vSandboxedClients.emplace_back(newClient);

    LOGM(LOG, "security_context at 0x{:x} got a new wl_client 0x{:x}", (uintptr_t)this, (uintptr_t)newClient->client);
}

void CSecurityContext::onClose(uint32_t mask) {
    if (!(mask & (WL_EVENT_ERROR | WL_EVENT_HANGUP)))
        return;

    PROTO::securityContext->destroyContext(this);
}

CSecurityContextManagerResource::CSecurityContextManagerResource(SP<CWpSecurityContextManagerV1> resource_) : resource(resource_) {
    if UNLIKELY (!good())
        return;

    resource->setDestroy([this](CWpSecurityContextManagerV1* r) { PROTO::securityContext->destroyResource(this); });
    resource->setOnDestroy([this](CWpSecurityContextManagerV1* r) { PROTO::securityContext->destroyResource(this); });

    resource->setCreateListener([](CWpSecurityContextManagerV1* r, uint32_t id, int32_t lfd, int32_t cfd) {
        const auto RESOURCE =
            PROTO::securityContext->m_vContexts.emplace_back(makeShared<CSecurityContext>(makeShared<CWpSecurityContextV1>(r->client(), r->version(), id), lfd, cfd));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::securityContext->m_vContexts.pop_back();
            return;
        }
    });
}

bool CSecurityContextManagerResource::good() {
    return resource->resource();
}

CSecurityContextProtocol::CSecurityContextProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CSecurityContextProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(makeShared<CSecurityContextManagerResource>(makeShared<CWpSecurityContextManagerV1>(client, ver, id)));

    if UNLIKELY (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_vManagers.pop_back();
        return;
    }
}

void CSecurityContextProtocol::destroyResource(CSecurityContextManagerResource* res) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other.get() == res; });
}

void CSecurityContextProtocol::destroyContext(CSecurityContext* context) {
    std::erase_if(m_vContexts, [&](const auto& other) { return other.get() == context; });
}

bool CSecurityContextProtocol::isClientSandboxed(const wl_client* client) {
    return std::find_if(m_vSandboxedClients.begin(), m_vSandboxedClients.end(), [client](const auto& e) { return e->client == client; }) != m_vSandboxedClients.end();
}
