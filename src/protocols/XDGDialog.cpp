#include "XDGDialog.hpp"
#include "XDGShell.hpp"
#include "../desktop/WLSurface.hpp"
#include "../Compositor.hpp"
#include <algorithm>

CXDGDialogV1Resource::CXDGDialogV1Resource(SP<CXdgDialogV1> resource_, SP<CXDGToplevelResource> toplevel_) : resource(resource_), toplevel(toplevel_) {
    if UNLIKELY (!good())
        return;

    resource->setDestroy([this](CXdgDialogV1* r) { PROTO::xdgDialog->destroyResource(this); });
    resource->setOnDestroy([this](CXdgDialogV1* r) { PROTO::xdgDialog->destroyResource(this); });

    resource->setSetModal([this](CXdgDialogV1* r) {
        modal = true;
        updateWindow();
    });

    resource->setUnsetModal([this](CXdgDialogV1* r) {
        modal = false;
        updateWindow();
    });
}

void CXDGDialogV1Resource::updateWindow() {
    if UNLIKELY (!toplevel || !toplevel->parent || !toplevel->parent->owner)
        return;

    auto HLSurface = CWLSurface::fromResource(toplevel->parent->owner->surface.lock());
    if UNLIKELY (!HLSurface || !HLSurface->getWindow())
        return;

    g_pCompositor->updateWindowAnimatedDecorationValues(HLSurface->getWindow());
}

bool CXDGDialogV1Resource::good() {
    return resource->resource();
}

CXDGWmDialogManagerResource::CXDGWmDialogManagerResource(SP<CXdgWmDialogV1> resource_) : resource(resource_) {
    if UNLIKELY (!good())
        return;

    resource->setDestroy([this](CXdgWmDialogV1* r) { PROTO::xdgDialog->destroyResource(this); });
    resource->setOnDestroy([this](CXdgWmDialogV1* r) { PROTO::xdgDialog->destroyResource(this); });

    resource->setGetXdgDialog([](CXdgWmDialogV1* r, uint32_t id, wl_resource* toplevel) {
        auto tl = CXDGToplevelResource::fromResource(toplevel);
        if UNLIKELY (!tl) {
            r->error(-1, "Toplevel inert");
            return;
        }

        const auto RESOURCE = PROTO::xdgDialog->m_vDialogs.emplace_back(makeShared<CXDGDialogV1Resource>(makeShared<CXdgDialogV1>(r->client(), r->version(), id), tl));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            return;
        }

        tl->dialog = RESOURCE;
    });
}

bool CXDGWmDialogManagerResource::good() {
    return resource->resource();
}

CXDGDialogProtocol::CXDGDialogProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CXDGDialogProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(makeShared<CXDGWmDialogManagerResource>(makeShared<CXdgWmDialogV1>(client, ver, id)));

    if UNLIKELY (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        return;
    }
}

void CXDGDialogProtocol::destroyResource(CXDGWmDialogManagerResource* res) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other.get() == res; });
}

void CXDGDialogProtocol::destroyResource(CXDGDialogV1Resource* res) {
    std::erase_if(m_vDialogs, [&](const auto& other) { return other.get() == res; });
}
