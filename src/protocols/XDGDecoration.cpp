#include "XDGDecoration.hpp"
#include <algorithm>

zxdgToplevelDecorationV1Mode xdgDefaultModeCSD(CXDGDecoration* csd) {
    return ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;
}

zxdgToplevelDecorationV1Mode xdgModeOnRequestCSD(CXDGDecoration* csd, uint32_t modeRequestedByClient) {
    return xdgDefaultModeCSD(csd);
}

zxdgToplevelDecorationV1Mode xdgModeOnReleaseCSD(CXDGDecoration* csd) {
    return xdgDefaultModeCSD(csd);
}

CXDGDecoration::CXDGDecoration(SP<CZxdgToplevelDecorationV1> resource_, wl_resource* toplevel) : m_resource(resource_), m_toplevelResource(toplevel) {
    if UNLIKELY (!m_resource->resource())
        return;

    m_resource->setDestroy([this](CZxdgToplevelDecorationV1* pMgr) { PROTO::xdgDecoration->destroyDecoration(this); });
    m_resource->setOnDestroy([this](CZxdgToplevelDecorationV1* pMgr) { PROTO::xdgDecoration->destroyDecoration(this); });

    m_resource->setSetMode([this](CZxdgToplevelDecorationV1*, zxdgToplevelDecorationV1Mode mode) {
        std::string modeString;
        switch (mode) {
            case ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE: modeString = "MODE_CLIENT_SIDE"; break;
            case ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE: modeString = "MODE_SERVER_SIDE"; break;
            default: modeString = "INVALID"; break;
        }

        LOGM(LOG, "setMode: {}. {} MODE_SERVER_SIDE as reply.", modeString, (mode == ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE ? "Sending" : "Ignoring and sending"));
        m_resource->sendConfigure(xdgModeOnRequestCSD(this, mode));
    });

    m_resource->setUnsetMode([this](CZxdgToplevelDecorationV1*) {
        LOGM(LOG, "unsetMode. Sending MODE_SERVER_SIDE.");
        m_resource->sendConfigure(xdgModeOnReleaseCSD(this));
    });

    m_resource->sendConfigure(xdgDefaultModeCSD(this));
}

bool CXDGDecoration::good() {
    return m_resource->resource();
}

wl_resource* CXDGDecoration::toplevelResource() {
    return m_toplevelResource;
}

CXDGDecorationProtocol::CXDGDecorationProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CXDGDecorationProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(makeUnique<CZxdgDecorationManagerV1>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CZxdgDecorationManagerV1* p) { this->onManagerResourceDestroy(p->resource()); });

    RESOURCE->setDestroy([this](CZxdgDecorationManagerV1* pMgr) { this->onManagerResourceDestroy(pMgr->resource()); });
    RESOURCE->setGetToplevelDecoration([this](CZxdgDecorationManagerV1* pMgr, uint32_t id, wl_resource* xdgToplevel) { this->onGetDecoration(pMgr, id, xdgToplevel); });
}

void CXDGDecorationProtocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_managers, [&](const auto& other) { return other->resource() == res; });
}

void CXDGDecorationProtocol::destroyDecoration(CXDGDecoration* decoration) {
    m_decorations.erase(decoration->toplevelResource());
}

void CXDGDecorationProtocol::onGetDecoration(CZxdgDecorationManagerV1* pMgr, uint32_t id, wl_resource* xdgToplevel) {
    if UNLIKELY (m_decorations.contains(xdgToplevel)) {
        pMgr->error(ZXDG_TOPLEVEL_DECORATION_V1_ERROR_ALREADY_CONSTRUCTED, "Decoration object already exists");
        return;
    }

    const auto CLIENT = pMgr->client();
    const auto RESOURCE =
        m_decorations.emplace(xdgToplevel, makeUnique<CXDGDecoration>(makeShared<CZxdgToplevelDecorationV1>(CLIENT, pMgr->version(), id), xdgToplevel)).first->second.get();

    if UNLIKELY (!RESOURCE->good()) {
        pMgr->noMemory();
        m_decorations.erase(xdgToplevel);
        return;
    }
}
