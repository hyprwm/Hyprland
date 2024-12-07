#pragma once

#include "../defines.hpp"
struct wl_event_source;

class CThreadManager {
  public:
    CThreadManager();
    ~CThreadManager();

    wl_event_source* m_esConfigTimer = nullptr;

  private:
};

inline std::unique_ptr<CThreadManager> g_pThreadManager;