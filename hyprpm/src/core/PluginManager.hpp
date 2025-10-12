#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <functional>

enum eHeadersErrors {
    HEADERS_OK = 0,
    HEADERS_NOT_HYPRLAND,
    HEADERS_MISSING,
    HEADERS_CORRUPTED,
    HEADERS_MISMATCHED,
    HEADERS_DUPLICATED
};

enum eNotifyIcons {
    ICON_WARNING = 0,
    ICON_INFO,
    ICON_HINT,
    ICON_ERROR,
    ICON_CONFUSED,
    ICON_OK,
    ICON_NONE
};

enum ePluginLoadStateReturn {
    LOADSTATE_OK = 0,
    LOADSTATE_FAIL,
    LOADSTATE_PARTIAL_FAIL,
    LOADSTATE_HEADERS_OUTDATED,
    LOADSTATE_HYPRLAND_UPDATED
};

struct SHyprlandVersion {
    std::string branch;
    std::string hash;
    std::string date;
    int         commits = 0;
};

class CPluginManager {
  public:
    CPluginManager();

    bool                   addNewPluginRepo(const std::string& url, const std::string& rev);
    bool                   removePluginRepo(const std::string& urlOrName);

    eHeadersErrors         headersValid();
    bool                   updateHeaders(bool force = false);
    bool                   updatePlugins(bool forceUpdateAll);

    void                   listAllPlugins();

    bool                   enablePlugin(const std::string& name);
    bool                   disablePlugin(const std::string& name);
    ePluginLoadStateReturn ensurePluginsLoadState(bool forceReload = false);

    bool                   loadUnloadPlugin(const std::string& path, bool load);
    SHyprlandVersion       getHyprlandVersion(bool running = true);

    void                   notify(const eNotifyIcons icon, uint32_t color, int durationMs, const std::string& message);

    bool                   hasDeps();

    bool                   devMode(const std::string& path = ".", bool hotReload = false);

    std::string            headerError(const eHeadersErrors err);
    bool                   buildPlugin(const std::string& path, class CManifest* manifest,
                                      std::function<bool()> shouldInterrupt = nullptr);

    bool                   m_bVerbose   = false;
    bool                   m_bNoShallow = false;
    std::string            m_szCustomHlUrl, m_szUsername;

    // will delete recursively if exists!!
    bool createSafeDirectory(const std::string& path);

  private:
    std::string headerErrorShort(const eHeadersErrors err);

    std::string m_szWorkingPluginDirectory;
};

inline std::unique_ptr<CPluginManager> g_pPluginManager;
