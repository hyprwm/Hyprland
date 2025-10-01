#include "CTMControl.hpp"
#include "../Compositor.hpp"
#include "../render/Renderer.hpp"
#include "core/Output.hpp"
#include "../config/ConfigValue.hpp"
#include "../config/ConfigManager.hpp"
#include "managers/AnimationManager.hpp"
#include "../helpers/Monitor.hpp"
#include "../helpers/MiscFunctions.hpp"

CHyprlandCTMControlResource::CHyprlandCTMControlResource(SP<CHyprlandCtmControlManagerV1> resource_) : m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setDestroy([this](CHyprlandCtmControlManagerV1* pMgr) { PROTO::ctm->destroyResource(this); });
    m_resource->setOnDestroy([this](CHyprlandCtmControlManagerV1* pMgr) { PROTO::ctm->destroyResource(this); });

    m_resource->setSetCtmForOutput([this](CHyprlandCtmControlManagerV1* r, wl_resource* output, wl_fixed_t mat0, wl_fixed_t mat1, wl_fixed_t mat2, wl_fixed_t mat3, wl_fixed_t mat4,
                                          wl_fixed_t mat5, wl_fixed_t mat6, wl_fixed_t mat7, wl_fixed_t mat8) {
        if (m_blocked)
            return;

        const auto OUTPUTRESOURCE = CWLOutputResource::fromResource(output);

        if UNLIKELY (!OUTPUTRESOURCE)
            return; // ?!

        const auto PMONITOR = OUTPUTRESOURCE->m_monitor.lock();

        if UNLIKELY (!PMONITOR)
            return; // ?!?!

        const std::array<float, 9> MAT = {wl_fixed_to_double(mat0), wl_fixed_to_double(mat1), wl_fixed_to_double(mat2), wl_fixed_to_double(mat3), wl_fixed_to_double(mat4),
                                          wl_fixed_to_double(mat5), wl_fixed_to_double(mat6), wl_fixed_to_double(mat7), wl_fixed_to_double(mat8)};

        for (auto& el : MAT) {
            if (!std::isfinite(el) || el < 0.F) {
                m_resource->error(HYPRLAND_CTM_CONTROL_MANAGER_V1_ERROR_INVALID_MATRIX, "a matrix component was invalid");
                return;
            }
        }

        m_ctms[PMONITOR->m_name] = MAT;

        LOGM(LOG, "CTM set for output {}: {}", PMONITOR->m_name, m_ctms.at(PMONITOR->m_name).toString());
    });

    m_resource->setCommit([this](CHyprlandCtmControlManagerV1* r) {
        if (m_blocked)
            return;

        LOGM(LOG, "Committing ctms to outputs");

        for (auto& m : g_pCompositor->m_monitors) {
            if (!m_ctms.contains(m->m_name)) {
                PROTO::ctm->setCTM(m, Mat3x3::identity());
                continue;
            }

            PROTO::ctm->setCTM(m, m_ctms.at(m->m_name));
        }
    });
}

void CHyprlandCTMControlResource::block() {
    m_blocked = true;

    if (m_resource->version() >= 2)
        m_resource->sendBlocked();
}

CHyprlandCTMControlResource::~CHyprlandCTMControlResource() {
    if (m_blocked)
        return;

    for (auto& m : g_pCompositor->m_monitors) {
        PROTO::ctm->setCTM(m, Mat3x3::identity());
    }
}

bool CHyprlandCTMControlResource::good() {
    return m_resource->resource();
}

CHyprlandCTMControlProtocol::CHyprlandCTMControlProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CHyprlandCTMControlProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(makeShared<CHyprlandCTMControlResource>(makeShared<CHyprlandCtmControlManagerV1>(client, ver, id)));

    if UNLIKELY (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_managers.pop_back();
        return;
    }

    if (m_manager)
        RESOURCE->block();
    else
        m_manager = RESOURCE;

    LOGM(LOG, "New CTM Manager at 0x{:x}", (uintptr_t)RESOURCE.get());
}

void CHyprlandCTMControlProtocol::destroyResource(CHyprlandCTMControlResource* res) {
    std::erase_if(m_managers, [&](const auto& other) { return other.get() == res; });
}

bool CHyprlandCTMControlProtocol::isCTMAnimationEnabled() {
    static auto PENABLEANIM = CConfigValue<Hyprlang::INT>("render:ctm_animation");

    if (*PENABLEANIM == 2 /* auto */) {
        if (!g_pHyprRenderer->isNvidia())
            return true;
        // CTM animations are bugged on versions below.
        return isNvidiaDriverVersionAtLeast(575);
    }
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

    std::erase_if(m_ctmDatas, [](const auto& el) { return !el.first; });

    if (!m_ctmDatas.contains(monitor))
        m_ctmDatas[monitor] = makeUnique<SCTMData>();

    auto& data = m_ctmDatas.at(monitor);

    data->ctmFrom = data->ctmTo;
    data->ctmTo   = ctm;

    data->progress->setValueAndWarp(0.F);
    *data->progress = 1.F;

    monitor->setCTM(data->ctmFrom);

    data->progress->setUpdateCallback([monitor = PHLMONITORREF{monitor}, this](auto) {
        if (!monitor || !m_ctmDatas.contains(monitor))
            return;
        auto&                data     = m_ctmDatas.at(monitor);
        const auto           from     = data->ctmFrom.getMatrix();
        const auto           to       = data->ctmTo.getMatrix();
        const auto           PROGRESS = data->progress->getPercent();

        static const auto    lerp = [](const float one, const float two, const float progress) -> float { return one + (two - one) * progress; };

        std::array<float, 9> mtx;
        for (size_t i = 0; i < 9; ++i) {
            mtx[i] = lerp(from[i], to[i], PROGRESS);
        }

        monitor->setCTM(mtx);
    });

    data->progress->setCallbackOnEnd([monitor = PHLMONITORREF{monitor}, this](auto) {
        if (!monitor || !m_ctmDatas.contains(monitor)) {
            if (monitor)
                monitor->setCTM(Mat3x3::identity());
            return;
        }
        auto& data = m_ctmDatas.at(monitor);
        monitor->setCTM(data->ctmTo);
    });
}
