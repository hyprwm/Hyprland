#pragma once

#include "../defines.hpp"
#include <thread>
#include "../Compositor.hpp"

class CThreadManager {
public:
    CThreadManager();
    ~CThreadManager();

private:

    void                handle();

    std::thread*        m_tMainThread;
};

inline std::unique_ptr<CThreadManager> g_pThreadManager;