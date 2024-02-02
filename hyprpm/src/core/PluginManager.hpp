#pragma once

#include <memory>
#include <string>

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
    LOADSTATE_HEADERS_OUTDATED
};

struct SHyprlandVersion {
    std::string branch;
    std::string hash;
};

class CPluginManager {
  public:
    bool                   addNewPluginRepo(const std::string& url);
    bool                   removePluginRepo(const std::string& urlOrName);

    eHeadersErrors         headersValid();
    bool                   updateHeaders(bool force = false);
    bool                   updatePlugins(bool forceUpdateAll);

    void                   listAllPlugins();

    bool                   enablePlugin(const std::string& name);
    bool                   disablePlugin(const std::string& name);
    ePluginLoadStateReturn ensurePluginsLoadState();

    bool                   loadUnloadPlugin(const std::string& path, bool load);
    SHyprlandVersion       getHyprlandVersion();

    void                   notify(const eNotifyIcons icon, uint32_t color, int durationMs, const std::string& message);

    bool                   m_bVerbose = false;

  private:
    std::string headerError(const eHeadersErrors err);
};

inline std::unique_ptr<CPluginManager> g_pPluginManager;