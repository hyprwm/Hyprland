#include "XDGDecoration.hpp"
#include <algorithm>

CXDGDecoration::CXDGDecoration(SP<CZxdgToplevelDecorationV1> resource_, wl_resource* toplevel) : resource(resource_), pToplevelResource(toplevel) {
    if (!resource->resource())
        return;

    resource->setDestroy([this](CZxdgToplevelDecorationV1* pMgr) { PROTO::xdgDecoration->destroyDecoration(this); });
    resource->setOnDestroy([this](CZxdgToplevelDecorationV1* pMgr) { PROTO::xdgDecoration->destroyDecoration(this); });

    resource->setSetMode([this](CZxdgToplevelDecorationV1*, zxdgToplevelDecorationV1Mode mode) {
        std::string modeString;
        switch (mode) {
            case ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE: modeString = "MODE_CLIENT_SIDE"; break;
            case ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE: modeString = "MODE_SERVER_SIDE"; break;
            default: modeString = "INVALID"; break;
        }

        LOGM(LOG, "setMode: {}. {} MODE_SERVER_SIDE as reply.", modeString, (mode == ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE ? "Sending" : "Ignoring and sending"));
        resource->sendConfigure(ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
    });

    resource->setUnsetMode([this](CZxdgToplevelDecorationV1*) {
        LOGM(LOG, "unsetMode. Sending MODE_SERVER_SIDE.");
        resource->sendConfigure(ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
    });

    resource->sendConfigure(ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
}

bool CXDGDecoration::good() {
    return resource->resource();
}

wl_resource* CXDGDecoration::toplevelResource() {
    return pToplevelResource;
}

CXDGDecorationProtocol::CXDGDecorationProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CXDGDecorationProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(std::make_unique<CZxdgDecorationManagerV1>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CZxdgDecorationManagerV1* p) { this->onManagerResourceDestroy(p->resource()); });

    RESOURCE->setDestroy([this](CZxdgDecorationManagerV1* pMgr) { this->onManagerResourceDestroy(pMgr->resource()); });
    RESOURCE->setGetToplevelDecoration([this](CZxdgDecorationManagerV1* pMgr, uint32_t id, wl_resource* xdgToplevel) { this->onGetDecoration(pMgr, id, xdgToplevel); });
}

void CXDGDecorationProtocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other->resource() == res; });
}

void CXDGDecorationProtocol::destroyDecoration(CXDGDecoration* decoration) {
    m_mDecorations.erase(decoration->toplevelResource());
}

void CXDGDecorationProtocol::onGetDecoration(CZxdgDecorationManagerV1* pMgr, uint32_t id, wl_resource* xdgToplevel) {
    if (m_mDecorations.contains(xdgToplevel)) {
        pMgr->error(ZXDG_TOPLEVEL_DECORATION_V1_ERROR_ALREADY_CONSTRUCTED, "Decoration object already exists");
        return;
    }

    const auto CLIENT = pMgr->client();
    const auto RESOURCE =
        m_mDecorations.emplace(xdgToplevel, std::make_unique<CXDGDecoration>(makeShared<CZxdgToplevelDecorationV1>(CLIENT, pMgr->version(), id), xdgToplevel)).first->second.get();

    if (!RESOURCE->good()) {
        pMgr->noMemory();
        m_mDecorations.erase(xdgToplevel);
        return;
    }
}