#pragma once

#include <memory>
#include <string>

enum eHeadersErrors
{
    HEADERS_OK,
    HEADERS_NOT_HYPRLAND,
    HEADERS_MISSING,
    HEADERS_CORRUPTED,
    HEADERS_MISMATCHED,
};

class CPluginManager {
  public:
    bool           addNewPluginRepo(const std::string& url);
    bool           removePluginRepo(const std::string& urlOrName);

    eHeadersErrors headersValid();
    bool           updateHeaders();
    bool           updatePlugins(bool forceUpdateAll);

    void           listAllPlugins();

    bool           enablePlugin(const std::string& name);
    bool           disablePlugin(const std::string& name);
    void           ensurePluginsLoadState();

    bool           loadUnloadPlugin(const std::string& path, bool load);
};

inline std::unique_ptr<CPluginManager> g_pPluginManager;