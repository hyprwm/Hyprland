#include "CTMControl.hpp"
#include "../Compositor.hpp"
#include "core/Output.hpp"

CHyprlandCTMControlResource::CHyprlandCTMControlResource(SP<CHyprlandCtmControlManagerV1> resource_) : resource(resource_) {
    if (!good())
        return;

    resource->setDestroy([this](CHyprlandCtmControlManagerV1* pMgr) { PROTO::ctm->destroyResource(this); });
    resource->setOnDestroy([this](CHyprlandCtmControlManagerV1* pMgr) { PROTO::ctm->destroyResource(this); });

    resource->setSetCtmForOutput([this](CHyprlandCtmControlManagerV1* r, wl_resource* output, wl_fixed_t mat0, wl_fixed_t mat1, wl_fixed_t mat2, wl_fixed_t mat3, wl_fixed_t mat4,
                                        wl_fixed_t mat5, wl_fixed_t mat6, wl_fixed_t mat7, wl_fixed_t mat8) {
        const auto OUTPUTRESOURCE = CWLOutputResource::fromResource(output);

        if (!OUTPUTRESOURCE)
            return; // ?!

        const auto PMONITOR = OUTPUTRESOURCE->monitor.lock();

        if (!PMONITOR)
            return; // ?!?!

        const std::array<float, 9> MAT = {wl_fixed_to_double(mat0), wl_fixed_to_double(mat1), wl_fixed_to_double(mat2), wl_fixed_to_double(mat3), wl_fixed_to_double(mat4),
                                          wl_fixed_to_double(mat5), wl_fixed_to_double(mat6), wl_fixed_to_double(mat7), wl_fixed_to_double(mat8)};

        for (auto& el : MAT) {
            if (el < 0.F) {
                resource->error(HYPRLAND_CTM_CONTROL_MANAGER_V1_ERROR_INVALID_MATRIX, "a matrix component was < 0");
                return;
            }
        }

        ctms[PMONITOR->szName] = MAT;

        LOGM(LOG, "CTM set for output {}: {}", PMONITOR->szName, ctms.at(PMONITOR->szName).toString());
    });

    resource->setCommit([this](CHyprlandCtmControlManagerV1* r) {
        LOGM(LOG, "Committing ctms to outputs");

        for (auto& m : g_pCompositor->m_vMonitors) {
            if (!ctms.contains(m->szName)) {
                PROTO::ctm->setCTM(m, Mat3x3::identity());
                continue;
            }

            PROTO::ctm->setCTM(m, ctms.at(m->szName));
        }
    });
}

CHyprlandCTMControlResource::~CHyprlandCTMControlResource() {
    for (auto& m : g_pCompositor->m_vMonitors) {
        PROTO::ctm->setCTM(m, Mat3x3::identity());
    }
}

bool CHyprlandCTMControlResource::good() {
    return resource->resource();
}

CHyprlandCTMControlProtocol::CHyprlandCTMControlProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CHyprlandCTMControlProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {

    const auto RESOURCE = m_vManagers.emplace_back(makeShared<CHyprlandCTMControlResource>(makeShared<CHyprlandCtmControlManagerV1>(client, ver, id)));

    if (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_vManagers.pop_back();
        return;
    }

    LOGM(LOG, "New CTM Manager at 0x{:x}", (uintptr_t)RESOURCE.get());
}

void CHyprlandCTMControlProtocol::destroyResource(CHyprlandCTMControlResource* res) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other.get() == res; });
}

void CHyprlandCTMControlProtocol::setCTM(SP<CMonitor> monitor, const Mat3x3& ctm) {
    monitor->setCTM(ctm);
}
