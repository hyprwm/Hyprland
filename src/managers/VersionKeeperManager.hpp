#pragma once

#include <memory>

class CVersionKeeperManager {
  public:
    CVersionKeeperManager();

    // whether the update screen was shown this boot.
    bool fired();

  private:
    bool isVersionOlderThanRunning(const std::string& ver);

    bool m_bFired = false;
};

inline std::unique_ptr<CVersionKeeperManager> g_pVersionKeeperMgr;