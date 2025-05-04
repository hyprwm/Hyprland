#pragma once

#include <vector>
#include <cstdint>
#include "WaylandProtocol.hpp"
#include "security-context-v1.hpp"
#include <hyprutils/os/FileDescriptor.hpp>

class CSecurityContext {
  public:
    CSecurityContext(SP<CWpSecurityContextV1> resource_, int listenFD_, int closeFD_);
    ~CSecurityContext();

    bool                           good();

    std::string                    m_sandboxEngine;
    std::string                    m_appID;
    std::string                    m_instanceID;

    Hyprutils::OS::CFileDescriptor m_listenFD;
    Hyprutils::OS::CFileDescriptor m_closeFD;

    void                           onListen(uint32_t mask);
    void                           onClose(uint32_t mask);

  private:
    SP<CWpSecurityContextV1> m_resource;

    wl_event_source*         m_listenSource = nullptr;
    wl_event_source*         m_closeSource  = nullptr;

    bool                     m_committed = false;
};

class CSecurityContextManagerResource {
  public:
    CSecurityContextManagerResource(SP<CWpSecurityContextManagerV1> resource_);

    bool good();

  private:
    SP<CWpSecurityContextManagerV1> m_resource;
};

class CSecurityContextSandboxedClient;
struct SCSecurityContextSandboxedClientDestroyWrapper {
    wl_listener                      listener;
    CSecurityContextSandboxedClient* parent = nullptr;
};

class CSecurityContextSandboxedClient {
  public:
    static SP<CSecurityContextSandboxedClient> create(Hyprutils::OS::CFileDescriptor clientFD);
    ~CSecurityContextSandboxedClient();

    void                                           onDestroy();

    SCSecurityContextSandboxedClientDestroyWrapper m_destroyListener;

  private:
    CSecurityContextSandboxedClient(Hyprutils::OS::CFileDescriptor clientFD_);

    wl_client*                     m_client = nullptr;
    Hyprutils::OS::CFileDescriptor m_clientFD;

    friend class CSecurityContextProtocol;
    friend class CSecurityContext;
};

class CSecurityContextProtocol : public IWaylandProtocol {
  public:
    CSecurityContextProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

    bool         isClientSandboxed(const wl_client* client);

  private:
    void destroyResource(CSecurityContextManagerResource* resource);

    void destroyContext(CSecurityContext* context);

    //
    std::vector<SP<CSecurityContextManagerResource>> m_managers;
    std::vector<SP<CSecurityContext>>                m_contexts;
    std::vector<SP<CSecurityContextSandboxedClient>> m_sandboxedClients;

    friend class CSecurityContextManagerResource;
    friend class CSecurityContext;
    friend class CSecurityContextSandboxedClient;
};

namespace PROTO {
    inline UP<CSecurityContextProtocol> securityContext;
};
