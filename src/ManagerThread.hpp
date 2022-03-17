#pragma once

#include "defines.hpp"
#include <thread>
#include "Compositor.hpp"

class CManagerThread {
public:
    CManagerThread();
    ~CManagerThread();

private:

    void                handle();

    std::thread*        m_tMainThread;
};

inline std::unique_ptr<CManagerThread> g_pManagerThread;