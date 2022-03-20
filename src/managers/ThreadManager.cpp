#include "ThreadManager.hpp"
#include "../debug/HyprCtl.hpp"

CThreadManager::CThreadManager() {
    m_tMainThread = new std::thread([&]() {
        // Call the handle method.
        this->handle();
    });

    m_tMainThread->detach(); // detach and continue.
}

CThreadManager::~CThreadManager() {
    //
}

int slowUpdate = 0;

void CThreadManager::handle() {

    g_pConfigManager->init();

    while (3.1415f) {
        slowUpdate++;
        if (slowUpdate >= g_pConfigManager->getInt("general:max_fps")){
            g_pConfigManager->tick();
            slowUpdate = 0;
        }

        HyprCtl::tickHyprCtl();

        std::this_thread::sleep_for(std::chrono::microseconds(1000000 / g_pConfigManager->getInt("general:max_fps")));
    }
}