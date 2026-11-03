#include "XDGActivation.hpp"
#include "../managers/TokenManager.hpp"
#include "../Compositor.hpp"
#include "../managers/SeatManager.hpp"
#include "core/Compositor.hpp"
#include <algorithm>

static constexpr const size_t MAX_TOKENS_IN_CACHE   = 1024;
static constexpr const size_t MAX_TOKENS_PER_CLIENT = 16;

CHyprlandActivationToken::CHyprlandActivationToken(wl_client* client, bool canActivate) :
    m_token(g_pTokenManager->registerNewToken({}, std::chrono::months{12})), m_canActivate(canActivate), m_client(client) {
    ;
}

CHyprlandActivationToken::~CHyprlandActivationToken() {
    g_pTokenManager->removeToken(g_pTokenManager->getToken(m_token));
}

bool CHyprlandActivationToken::canActivate() const {
    return m_canActivate;
}

const std::string& CHyprlandActivationToken::token() const {
    return m_token;
}

wl_client* CHyprlandActivationToken::client() const {
    return m_client;
}

CXDGActivationToken::CXDGActivationToken(SP<CXdgActivationTokenV1> resource_) : m_resource(resource_) {
    if UNLIKELY (!resource_->resource())
        return;

    m_resource->setDestroy([this](CXdgActivationTokenV1* r) { PROTO::activation->destroyToken(this); });
    m_resource->setOnDestroy([this](CXdgActivationTokenV1* r) { PROTO::activation->destroyToken(this); });

    m_resource->setSetSerial([this](CXdgActivationTokenV1* r, uint32_t serial_, wl_resource* seat) { m_serial = serial_; });

    m_resource->setSetAppId([this](CXdgActivationTokenV1* r, const char* appid) { m_appID = appid; });

    m_resource->setCommit([this](CXdgActivationTokenV1* r) {
        auto rej = [this] {
            // is this fucking correct? Or should we just never send done???
            m_resource->sendDone("");
        };

        // TODO: should we send a protocol error of already_used here
        // if it was used? the protocol spec doesn't say _when_ it should be sent...
        if UNLIKELY (m_committed) {
            LOG(Log::WARN, "possible protocol error, two commits from one token, rejecting");
            rej();
            return;
        }

        m_committed = true;

        const bool CAN_ACTIVATE = m_serial && g_pSeatManager->serialValid(g_pSeatManager->seatResourceForClient(m_resource->client()), m_serial);

        // send done with a new token
        const auto& TOKEN = PROTO::activation->m_sentTokens.emplace_back(makeUnique<CHyprlandActivationToken>(m_resource->client(), CAN_ACTIVATE));

        LOG(Log::DEBUG, "assigned new xdg-activation token {}", TOKEN->token());

        if (!CAN_ACTIVATE)
            LOG(Log::DEBUG, "new xdg activation token {} has no activation rights, will be urgent-only", TOKEN->token());

        m_resource->sendDone(TOKEN->token().c_str());
        PROTO::activation->cleanupTokens();
    });
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
        auto TOKEN = std::ranges::find_if(m_sentTokens, [token](const auto& t) { return t->token() == token; });

        if UNLIKELY (TOKEN == m_sentTokens.end()) {
            LOG(Log::WARN, "activate event for non-existent token {}??", token);
            return;
        }

        const bool CAN_ACTIVATE = (*TOKEN)->canActivate();

        // remove token. It's been now spent.
        m_sentTokens.erase(TOKEN);

        SP<CWLSurfaceResource> surf    = CWLSurfaceResource::fromResource(surface);
        const auto             PWINDOW = Desktop::viewState()->query().type(Desktop::View::VIEW_TYPE_WINDOW).surface(surf).runWindow();

        if UNLIKELY (!PWINDOW) {
            LOG(Log::WARN, "activate event for non-window or gone surface with token {}, ignoring", token);
            return;
        }

        if (CAN_ACTIVATE)
            PWINDOW->activate();
        else
            PWINDOW->markUrgent();
    });
}

void CXDGActivationProtocol::cleanupTokens() {
    if (m_sentTokens.empty())
        return;

    // Called after each insertion, so only the newest token's client can exceed its limit.
    const auto CLIENT = m_sentTokens.back()->client();
    size_t     count  = std::ranges::count_if(m_sentTokens, [CLIENT](const auto& token) { return token->client() == CLIENT; });

    std::erase_if(m_sentTokens, [CLIENT, &count](const auto& token) {
        if (token->client() != CLIENT || count <= MAX_TOKENS_PER_CLIENT)
            return false;

        --count;
        return true;
    });

    if (m_sentTokens.size() > MAX_TOKENS_IN_CACHE)
        m_sentTokens.erase(m_sentTokens.begin(), m_sentTokens.begin() + (m_sentTokens.size() - MAX_TOKENS_IN_CACHE));
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
