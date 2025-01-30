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

    std::string                    sandboxEngine, appID, instanceID;
    Hyprutils::OS::CFileDescriptor listenFD, closeFD;

    void                           onListen(uint32_t mask);
    void                           onClose(uint32_t mask);

  private:
    SP<CWpSecurityContextV1> resource;

    wl_event_source *        listenSource = nullptr, *closeSource = nullptr;

    bool                     committed = false;
};

class CSecurityContextManagerResource {
  public:
    CSecurityContextManagerResource(SP<CWpSecurityContextManagerV1> resource_);

    bool good();

  private:
    SP<CWpSecurityContextManagerV1> resource;
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

    SCSecurityContextSandboxedClientDestroyWrapper destroyListener;

  private:
    CSecurityContextSandboxedClient(Hyprutils::OS::CFileDescriptor clientFD_);

    wl_client*                     client = nullptr;
    Hyprutils::OS::CFileDescriptor clientFD;

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
    std::vector<SP<CSecurityContextManagerResource>> m_vManagers;
    std::vector<SP<CSecurityContext>>                m_vContexts;
    std::vector<SP<CSecurityContextSandboxedClient>> m_vSandboxedClients;

    friend class CSecurityContextManagerResource;
    friend class CSecurityContext;
    friend class CSecurityContextSandboxedClient;
};

namespace PROTO {
    inline UP<CSecurityContextProtocol> securityContext;
};
