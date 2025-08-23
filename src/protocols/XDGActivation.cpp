#include "XDGActivation.hpp"
#include "../managers/TokenManager.hpp"
#include "../Compositor.hpp"
#include "../managers/SeatManager.hpp"
#include "core/Compositor.hpp"
#include <algorithm>

CXDGActivationToken::CXDGActivationToken(SP<CXdgActivationTokenV1> resource_) : m_resource(resource_) {
    if UNLIKELY (!resource_->resource())
        return;

    m_resource->setDestroy([this](CXdgActivationTokenV1* r) { PROTO::activation->destroyToken(this); });
    m_resource->setOnDestroy([this](CXdgActivationTokenV1* r) { PROTO::activation->destroyToken(this); });

    m_resource->setSetSerial([this](CXdgActivationTokenV1* r, uint32_t serial_, wl_resource* seat) { m_serial = serial_; });

    m_resource->setSetAppId([this](CXdgActivationTokenV1* r, const char* appid) { m_appID = appid; });

    m_resource->setCommit([this](CXdgActivationTokenV1* r) {
        // TODO: should we send a protocol error of already_used here
        // if it was used? the protocol spec doesn't say _when_ it should be sent...
        if UNLIKELY (m_committed) {
            LOGM(WARN, "possible protocol error, two commits from one token. Ignoring.");
            return;
        }

        if (!g_pSeatManager->serialValid(g_pSeatManager->seatResourceForClient(m_resource->client()), m_serial)) {
            LOGM(LOG, "serial not found, sending invalid token");
            m_resource->sendDone("__invalid__");
            return;
        }

        m_committed = true;
        // send done with a new token
        m_token = g_pTokenManager->registerNewToken({}, std::chrono::months{12});

        LOGM(LOG, "assigned new xdg-activation token {}", m_token);

        m_resource->sendDone(m_token.c_str());

        PROTO::activation->m_sentTokens.push_back({m_token, m_resource->client()});

        auto count = std::ranges::count_if(PROTO::activation->m_sentTokens, [this](const auto& other) { return other.client == m_resource->client(); });

        if UNLIKELY (count > 10) {
            // remove first token. Too many, dear app.
            for (auto i = PROTO::activation->m_sentTokens.begin(); i != PROTO::activation->m_sentTokens.end(); ++i) {
                if (i->client == m_resource->client()) {
                    PROTO::activation->m_sentTokens.erase(i);
                    break;
                }
            }
        }
    });
}

CXDGActivationToken::~CXDGActivationToken() {
    if (m_committed)
        g_pTokenManager->removeToken(g_pTokenManager->getToken(m_token));
}

bool CXDGActivationToken::good() {
    return m_resource->resource();
}

CXDGActivationProtocol::CXDGActivationProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CXDGActivationProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(makeUnique<CXdgActivationV1>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CXdgActivationV1* p) { this->onManagerResourceDestroy(p->resource()); });

    RESOURCE->setDestroy([this](CXdgActivationV1* pMgr) { this->onManagerResourceDestroy(pMgr->resource()); });
    RESOURCE->setGetActivationToken([this](CXdgActivationV1* pMgr, uint32_t id) { this->onGetToken(pMgr, id); });
    RESOURCE->setActivate([this](CXdgActivationV1* pMgr, const char* token, wl_resource* surface) {
        auto TOKEN = std::ranges::find_if(m_sentTokens, [token](const auto& t) { return t.token == token; });

        if UNLIKELY (TOKEN == m_sentTokens.end()) {
            LOGM(WARN, "activate event for non-existent token {}??", token);
            return;
        }

        // remove token. It's been now spent.
        m_sentTokens.erase(TOKEN);

        SP<CWLSurfaceResource> surf    = CWLSurfaceResource::fromResource(surface);
        const auto             PWINDOW = g_pCompositor->getWindowFromSurface(surf);

        if UNLIKELY (!PWINDOW) {
            LOGM(WARN, "activate event for non-window or gone surface with token {}, ignoring", token);
            return;
        }

        PWINDOW->activate();
    });
}

void CXDGActivationProtocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_managers, [&](const auto& other) { return other->resource() == res; });
}

void CXDGActivationProtocol::destroyToken(CXDGActivationToken* token) {
    std::erase_if(m_tokens, [&](const auto& other) { return other.get() == token; });
}

void CXDGActivationProtocol::onGetToken(CXdgActivationV1* pMgr, uint32_t id) {
    const auto CLIENT   = pMgr->client();
    const auto RESOURCE = m_tokens.emplace_back(makeUnique<CXDGActivationToken>(makeShared<CXdgActivationTokenV1>(CLIENT, pMgr->version(), id))).get();

    if UNLIKELY (!RESOURCE->good()) {
        pMgr->noMemory();
        m_tokens.pop_back();
        return;
    }
}