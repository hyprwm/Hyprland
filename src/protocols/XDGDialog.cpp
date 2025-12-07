#include "XDGDialog.hpp"
#include "XDGShell.hpp"
#include "../desktop/WLSurface.hpp"
#include "../Compositor.hpp"
#include <algorithm>

CXDGDialogV1Resource::CXDGDialogV1Resource(SP<CXdgDialogV1> resource_, SP<CXDGToplevelResource> toplevel_) : m_resource(resource_), m_toplevel(toplevel_) {
    if UNLIKELY (!good())
        return;

    m_resource->setDestroy([this](CXdgDialogV1* r) { PROTO::xdgDialog->destroyResource(this); });
    m_resource->setOnDestroy([this](CXdgDialogV1* r) { PROTO::xdgDialog->destroyResource(this); });

    m_resource->setSetModal([this](CXdgDialogV1* r) {
        modal = true;
        updateWindow();
    });

    m_resource->setUnsetModal([this](CXdgDialogV1* r) {
        modal = false;
        updateWindow();
    });
}

void CXDGDialogV1Resource::updateWindow() {
    if UNLIKELY (!m_toplevel || !m_toplevel->m_parent || !m_toplevel->m_parent->m_owner)
        return;

    auto HLSurface = CWLSurface::fromResource(m_toplevel->m_parent->m_owner->m_surface.lock());
    if UNLIKELY (!HLSurface || !HLSurface->getWindow())
        return;

    HLSurface->getWindow()->m_ruleApplicator->propertiesChanged(Desktop::Rule::RULE_PROP_MODAL);
}

bool CXDGDialogV1Resource::good() {
    return m_resource->resource();
}

CXDGWmDialogManagerResource::CXDGWmDialogManagerResource(SP<CXdgWmDialogV1> resource_) : m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setDestroy([this](CXdgWmDialogV1* r) { PROTO::xdgDialog->destroyResource(this); });
    m_resource->setOnDestroy([this](CXdgWmDialogV1* r) { PROTO::xdgDialog->destroyResource(this); });

    m_resource->setGetXdgDialog([](CXdgWmDialogV1* r, uint32_t id, wl_resource* toplevel) {
        auto tl = CXDGToplevelResource::fromResource(toplevel);
        if UNLIKELY (!tl) {
            r->error(-1, "Toplevel inert");
            return;
        }

        const auto RESOURCE = PROTO::xdgDialog->m_dialogs.emplace_back(makeShared<CXDGDialogV1Resource>(makeShared<CXdgDialogV1>(r->client(), r->version(), id), tl));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            return;
        }

        tl->m_dialog = RESOURCE;
    });
}

bool CXDGWmDialogManagerResource::good() {
    return m_resource->resource();
}

CXDGDialogProtocol::CXDGDialogProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CXDGDialogProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(makeShared<CXDGWmDialogManagerResource>(makeShared<CXdgWmDialogV1>(client, ver, id)));

    if UNLIKELY (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        return;
    }
}

void CXDGDialogProtocol::destroyResource(CXDGWmDialogManagerResource* res) {
    std::erase_if(m_managers, [&](const auto& other) { return other.get() == res; });
}

void CXDGDialogProtocol::destroyResource(CXDGDialogV1Resource* res) {
    std::erase_if(m_dialogs, [&](const auto& other) { return other.get() == res; });
}
