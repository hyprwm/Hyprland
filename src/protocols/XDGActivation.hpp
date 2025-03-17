#pragma once

#include <vector>
#include <cstdint>
#include "WaylandProtocol.hpp"
#include "xdg-activation-v1.hpp"

class CXDGActivationToken {
  public:
    CXDGActivationToken(SP<CXdgActivationTokenV1> resource_);
    ~CXDGActivationToken();

    bool good();

  private:
    SP<CXdgActivationTokenV1> resource;

    uint32_t                  serial    = 0;
    std::string               appID     = "";
    bool                      committed = false;

    std::string               token = "";

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

    struct SSentToken {
        std::string token;
        wl_client*  client = nullptr; // READ-ONLY: can be dead
    };
    std::vector<SSentToken> m_vSentTokens;

    //
    std::vector<UP<CXdgActivationV1>>    m_vManagers;
    std::vector<UP<CXDGActivationToken>> m_vTokens;

    friend class CXDGActivationToken;
};

namespace PROTO {
    inline UP<CXDGActivationProtocol> activation;
};
