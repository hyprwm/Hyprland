#include "ThreadManager.hpp"
#include "../debug/HyprCtl.hpp"
#include "../Compositor.hpp"
#include "../config/ConfigValue.hpp"

int handleTimer(void* data) {
    const auto  PTM = (CThreadManager*)data;

    static auto PDISABLECFGRELOAD = CConfigValue<Hyprlang::INT>("misc:disable_autoreload");

    if (*PDISABLECFGRELOAD != 1)
        g_pConfigManager->tick();

    wl_event_source_timer_update(PTM->m_esConfigTimer, 1000);

    return 0;
}

CThreadManager::CThreadManager() {
    m_esConfigTimer = wl_event_loop_add_timer(g_pCompositor->m_sWLEventLoop, handleTimer, this);

    wl_event_source_timer_update(m_esConfigTimer, 1000);
}

CThreadManager::~CThreadManager() {
    if (m_esConfigTimer)
        wl_event_source_remove(m_esConfigTimer);
}
