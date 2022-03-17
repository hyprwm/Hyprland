#include "ManagerThread.hpp"

CManagerThread::CManagerThread() {
    m_tMainThread = new std::thread([=]() {
        // Call the handle method.
        this->handle();
    });

    m_tMainThread->detach(); // detach and continue.
}

CManagerThread::~CManagerThread() {
    //
}

void CManagerThread::handle() {

    g_pConfigManager->init();

    while (3.1415f) {
        g_pConfigManager->tick();

        std::this_thread::sleep_for(std::chrono::microseconds(1000000 / g_pConfigManager->getInt("max_fps")));
    }
}