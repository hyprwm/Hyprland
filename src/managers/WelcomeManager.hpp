#pragma once

#include "../helpers/memory/Memory.hpp"

class CWelcomeManager {
  public:
    CWelcomeManager();

    // whether the welcome screen was shown this boot.
    bool fired();

  private:
    bool m_fired = false;
};

inline UP<CWelcomeManager> g_pWelcomeManager;