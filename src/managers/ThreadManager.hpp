#pragma once

#include "../defines.hpp"
#include <thread>
#include "../Compositor.hpp"

class CThreadManager {
  public:
    CThreadManager();
    ~CThreadManager();

    wl_event_source* m_esConfigTimer;

  private:
};

inline std::unique_ptr<CThreadManager> g_pThreadManager;