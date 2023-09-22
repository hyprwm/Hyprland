#pragma once

#include <memory>
#include <thread>

#include "../common.hpp"


class CThreadManager {
  public:
    CThreadManager();
    ~CThreadManager();

    wl_event_source* m_esConfigTimer;

  private:
};

inline std::unique_ptr<CThreadManager> g_pThreadManager;
