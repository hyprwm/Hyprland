#pragma once

#include <string>

class CPluginManager;
class CManifest;

class CDevMode {
  public:
    CDevMode(CPluginManager* pluginManager);

    bool run(const std::string& path, bool hotReload);

  private:
    bool runNestedSession(const std::string& path, CManifest* pManifest, const std::string& absPath, const std::string& repoName);
    bool runHotReload(const std::string& path, CManifest* pManifest, const std::string& absPath, const std::string& repoName);

    // Nested session helpers
    static std::string getNestedSessionPidFile();
    static void        killExistingNestedSession();
    static bool        launchNestedSession(const std::string& pluginPath);

    CPluginManager* m_pPluginManager;
};
