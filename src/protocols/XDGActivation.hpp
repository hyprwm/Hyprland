#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include "WaylandProtocol.hpp"
#include "xdg-activation-v1.hpp"
#include "../macros/Class.hpp"

class CHyprlandActivationToken {
  public:
    CHyprlandActivationToken(wl_client* client, bool canActivate);
    ~CHyprlandActivationToken();

    NON_MOVABLE(CHyprlandActivationToken);

    bool               canActivate() const;
    const std::string& token() const;
    wl_client*         client() const;

  private:
    std::string m_token;
    bool        m_canActivate = false;
    wl_client*  m_client      = nullptr; // READ-ONLY: can be dead
};

class CXDGActivationToken {
  public:
    CXDGActivationToken(SP<CXdgActivationTokenV1> resource_);

    bool good();

  private:
    SP<CXdgActivationTokenV1> m_resource;

    uint32_t                  m_serial    = 0;
    std::string               m_appID     = "";
    bool                      m_committed = false;

    friend class CXDGActivationProtocol;
};

class CXDGActivationProtocol : public IWaylandProtocol {
  public:
    CXDGActivationProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

  private:
    void onManagerResourceDestroy(wl_resource* res);
    void destroyToken(CXDGActivationToken* pointer);
    void onGetToken(CXdgActivationV1* pMgr, uint32_t id);

    // enforces per-client and global token cache limits
    void cleanupTokens();

    // tokens sent to apps
    std::vector<UP<CHyprlandActivationToken>> m_sentTokens;

    //
    std::vector<UP<CXdgActivationV1>>    m_managers;
    std::vector<UP<CXDGActivationToken>> m_tokens;

    friend class CXDGActivationToken;
};

namespace PROTO {
    inline UP<CXDGActivationProtocol> activation;
};
