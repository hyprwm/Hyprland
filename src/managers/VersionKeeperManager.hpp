#pragma once

#include "../helpers/memory/Memory.hpp"

class CVersionKeeperManager {
  public:
    CVersionKeeperManager();

    // whether the update screen was shown this boot.
    bool fired();

  private:
    bool isMajorVersionOlderThanRunning(const std::string& ver);

    bool m_fired = false;
};

inline UP<CVersionKeeperManager> g_pVersionKeeperMgr;