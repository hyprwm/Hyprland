#include "CTMControl.hpp"
#include "../Compositor.hpp"
#include "../render/Renderer.hpp"
#include "core/Output.hpp"
#include "../config/ConfigValue.hpp"
#include "managers/AnimationManager.hpp"

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

bool CHyprlandCTMControlProtocol::isCTMAnimationEnabled() {
    static auto PENABLEANIM = CConfigValue<Hyprlang::INT>("render:ctm_animation");

    if (*PENABLEANIM == 2)
        return !g_pHyprRenderer->isNvidia();
    return *PENABLEANIM;
}

CHyprlandCTMControlProtocol::SCTMData::SCTMData() {
    g_pAnimationManager->createAnimation(0.f, progress, g_pConfigManager->getAnimationPropertyConfig("__internal_fadeCTM"), AVARDAMAGE_NONE);
}

void CHyprlandCTMControlProtocol::setCTM(PHLMONITOR monitor, const Mat3x3& ctm) {
    if (!isCTMAnimationEnabled()) {
        monitor->setCTM(ctm);
        return;
    }

    std::erase_if(m_mCTMDatas, [](const auto& el) { return !el.first; });

    if (!m_mCTMDatas.contains(monitor))
        m_mCTMDatas[monitor] = std::make_unique<SCTMData>();

    auto& data = m_mCTMDatas.at(monitor);

    data->ctmFrom = data->ctmTo;
    data->ctmTo   = ctm;

    data->progress.setValueAndWarp(0.F);
    data->progress = 1.F;

    monitor->setCTM(data->ctmFrom);

    data->progress.setUpdateCallback([monitor = PHLMONITORREF{monitor}, this](void* self) {
        if (!monitor || !m_mCTMDatas.contains(monitor))
            return;
        auto&                data     = m_mCTMDatas.at(monitor);
        const auto           from     = data->ctmFrom.getMatrix();
        const auto           to       = data->ctmTo.getMatrix();
        const auto           PROGRESS = data->progress.getPercent();

        static const auto    lerp = [](const float one, const float two, const float progress) -> float { return one + (two - one) * progress; };

        std::array<float, 9> mtx;
        for (size_t i = 0; i < 9; ++i) {
            mtx[i] = lerp(from[i], to[i], PROGRESS);
        }

        monitor->setCTM(mtx);
    });

    data->progress.setCallbackOnEnd([monitor = PHLMONITORREF{monitor}, this](void* self) {
        if (!monitor || !m_mCTMDatas.contains(monitor)) {
            monitor->setCTM(Mat3x3::identity());
            return;
        }
        auto& data = m_mCTMDatas.at(monitor);
        monitor->setCTM(data->ctmTo);
    });
}
