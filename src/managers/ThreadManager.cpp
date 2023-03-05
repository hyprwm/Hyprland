#include "ThreadManager.hpp"
#include "../debug/HyprCtl.hpp"
#include "../Compositor.hpp"

int slowUpdate = 0;

int handleTimer(void* data) {
    const auto         PTM = (CThreadManager*)data;

    static auto* const PDISABLECFGRELOAD = &g_pConfigManager->getConfigValuePtr("misc:disable_autoreload")->intValue;

    if (*PDISABLECFGRELOAD != 1)
        g_pConfigManager->tick();

    wl_event_source_timer_update(PTM->m_esConfigTimer, 1000);

    return 0;
}

CThreadManager::CThreadManager() {
    HyprCtl::startHyprCtlSocket();

    m_esConfigTimer = wl_event_loop_add_timer(g_pCompositor->m_sWLEventLoop, handleTimer, this);

    wl_event_source_timer_update(m_esConfigTimer, 1000);
}

CThreadManager::~CThreadManager() {
    //
}