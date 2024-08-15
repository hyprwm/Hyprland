#include "XDGActivation.hpp"
#include "../managers/TokenManager.hpp"
#include "../Compositor.hpp"
#include "core/Compositor.hpp"
#include <algorithm>

CXDGActivationToken::CXDGActivationToken(SP<CXdgActivationTokenV1> resource_) : resource(resource_) {
    if (!resource_->resource())
        return;

    resource->setDestroy([this](CXdgActivationTokenV1* r) { PROTO::activation->destroyToken(this); });
    resource->setOnDestroy([this](CXdgActivationTokenV1* r) { PROTO::activation->destroyToken(this); });

    resource->setSetSerial([this](CXdgActivationTokenV1* r, uint32_t serial_, wl_resource* seat) { serial = serial_; });

    resource->setSetAppId([this](CXdgActivationTokenV1* r, const char* appid) { appID = appid; });

    resource->setCommit([this](CXdgActivationTokenV1* r) {
        // TODO: should we send a protocol error of already_used here
        // if it was used? the protocol spec doesn't say _when_ it should be sent...
        if (committed) {
            LOGM(WARN, "possible protocol error, two commits from one token. Ignoring.");
            return;
        }

        committed = true;
        // send done with a new token
        token = g_pTokenManager->registerNewToken({}, std::chrono::months{12});

        LOGM(LOG, "assigned new xdg-activation token {}", token);

        resource->sendDone(token.c_str());

        PROTO::activation->m_vSentTokens.push_back({token, resource->client()});

        auto count = std::count_if(PROTO::activation->m_vSentTokens.begin(), PROTO::activation->m_vSentTokens.end(),
                                   [this](const auto& other) { return other.client == resource->client(); });

        if (count > 10) {
            // remove first token. Too many, dear app.
            for (auto i = PROTO::activation->m_vSentTokens.begin(); i != PROTO::activation->m_vSentTokens.end(); ++i) {
                if (i->client == resource->client()) {
                    PROTO::activation->m_vSentTokens.erase(i);
                    break;
                }
            }
        }
    });
}

CXDGActivationToken::~CXDGActivationToken() {
    if (committed)
        g_pTokenManager->removeToken(g_pTokenManager->getToken(token));
}

bool CXDGActivationToken::good() {
    return resource->resource();
}

CXDGActivationProtocol::CXDGActivationProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CXDGActivationProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(std::make_unique<CXdgActivationV1>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CXdgActivationV1* p) { this->onManagerResourceDestroy(p->resource()); });

    RESOURCE->setDestroy([this](CXdgActivationV1* pMgr) { this->onManagerResourceDestroy(pMgr->resource()); });
    RESOURCE->setGetActivationToken([this](CXdgActivationV1* pMgr, uint32_t id) { this->onGetToken(pMgr, id); });
    RESOURCE->setActivate([this](CXdgActivationV1* pMgr, const char* token, wl_resource* surface) {
        auto TOKEN = std::find_if(m_vSentTokens.begin(), m_vSentTokens.end(), [token](const auto& t) { return t.token == token; });

        if (TOKEN == m_vSentTokens.end()) {
            LOGM(WARN, "activate event for non-existent token {}??", token);
            return;
        }

        // remove token. It's been now spent.
        m_vSentTokens.erase(TOKEN);

        SP<CWLSurfaceResource> surf    = CWLSurfaceResource::fromResource(surface);
        const auto             PWINDOW = g_pCompositor->getWindowFromSurface(surf);

        if (!PWINDOW) {
            LOGM(WARN, "activate event for non-window or gone surface with token {}, ignoring", token);
            return;
        }

        PWINDOW->activate();
    });
}

void CXDGActivationProtocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other->resource() == res; });
}

void CXDGActivationProtocol::destroyToken(CXDGActivationToken* token) {
    std::erase_if(m_vTokens, [&](const auto& other) { return other.get() == token; });
}

void CXDGActivationProtocol::onGetToken(CXdgActivationV1* pMgr, uint32_t id) {
    const auto CLIENT   = pMgr->client();
    const auto RESOURCE = m_vTokens.emplace_back(std::make_unique<CXDGActivationToken>(makeShared<CXdgActivationTokenV1>(CLIENT, pMgr->version(), id))).get();

    if (!RESOURCE->good()) {
        pMgr->noMemory();
        m_vTokens.pop_back();
        return;
    }
}