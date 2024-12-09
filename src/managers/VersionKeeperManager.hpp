#pragma once

#include <memory>
#include <optional>

class CVersionKeeperManager {
  public:
    CVersionKeeperManager();

  private:
    std::optional<std::string> getDataHome();
    std::optional<std::string> getDataLastVersion(const std::string& dataRoot);
    void                       writeVersionToVersionFile(const std::string& dataRoot);
    bool                       isVersionOlderThanRunning(const std::string& ver);
};

inline std::unique_ptr<CVersionKeeperManager> g_pVersionKeeperMgr;