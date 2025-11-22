#pragma once

/*

Hyprland Plugin API.

Most documentation will be made with comments in this code, but more info can be also found on the wiki.

!WARNING!
The Hyprland API passes C++ objects over, so no ABI compatibility is guaranteed.
Make sure to compile your plugins with the same compiler as Hyprland, and ideally,
on the same machine.

See examples/examplePlugin for an example plugin

 * NOTE:
Feel like the API is missing something you'd like to use in your plugin? Open an issue on github!

*/

#define HYPRLAND_API_VERSION "0.1"

#include "../helpers/Color.hpp"
#include "HookSystem.hpp"
#include "../SharedDefs.hpp"
#include "../defines.hpp"
#include "../version.h"

#include <any>
#include <functional>
#include <string>
#include <string_view>
#include <hyprlang.hpp>

using PLUGIN_DESCRIPTION_INFO = struct {
    std::string name;
    std::string description;
    std::string author;
    std::string version;
};

struct SFunctionMatch {
    void*       address = nullptr;
    std::string signature;
    std::string demangled;
};

struct SVersionInfo {
    std::string hash;
    std::string tag;
    bool        dirty = false;
    std::string branch;
    std::string message;
    std::string commits;
};

#define APICALL extern "C"
#define EXPORT  __attribute__((visibility("default")))
#define REQUIRED
#define OPTIONAL
#define HANDLE void*

// C ABI is needed to prevent symbol mangling, but we don't actually need C compatibility,
// so we ignore this warning about return types that are potentially incompatible with C.
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreturn-type-c-linkage"
#endif

class IHyprLayout;
class CWindow;
class IHyprWindowDecoration;
struct SConfigValue;
class CWindow;

/*
    These methods are for the plugin to implement
    Methods marked with REQUIRED are required.
*/

/*
    called pre-plugin init.
    In case of a version mismatch, will eject the .so.

    This function should not be modified, see the example plugin.
*/
using PPLUGIN_API_VERSION_FUNC = REQUIRED std::string (*)();
#define PLUGIN_API_VERSION          pluginAPIVersion
#define PLUGIN_API_VERSION_FUNC_STR "pluginAPIVersion"

/*
    called on plugin init. Passes a handle as the parameter, which the plugin should keep for identification later.
    The plugin should return a PLUGIN_DESCRIPTION_INFO struct with information about itself.

    Keep in mind this is executed synchronously, and as such any blocking calls to hyprland might hang. (e.g. system("hyprctl ..."))
*/
using PPLUGIN_INIT_FUNC = REQUIRED PLUGIN_DESCRIPTION_INFO (*)(HANDLE);
#define PLUGIN_INIT          pluginInit
#define PLUGIN_INIT_FUNC_STR "pluginInit"

/*
    called on plugin unload, if that was a user action. If the plugin is being unloaded by an error,
    this will not be called.

    Hooks are unloaded after exit.
*/
using PPLUGIN_EXIT_FUNC = OPTIONAL void (*)();
#define PLUGIN_EXIT          pluginExit
#define PLUGIN_EXIT_FUNC_STR "pluginExit"

/*
    End plugin methods
*/

// NOLINTNEXTLINE(readability-identifier-naming)
namespace HyprlandAPI {

    /*
        Add a config value.
        All config values MUST be in the plugin: namespace
        This method may only be called in "pluginInit"

        After you have registered ALL of your config values, you may call `getConfigValue`

        returns: true on success, false on fail
    */
    APICALL bool addConfigValue(HANDLE handle, const std::string& name, const Hyprlang::CConfigValue& value);

    /*
        Add a config keyword.
        This method may only be called in "pluginInit"

        returns: true on success, false on fail
    */
    APICALL bool addConfigKeyword(HANDLE handle, const std::string& name, Hyprlang::PCONFIGHANDLERFUNC fn, Hyprlang::SHandlerOptions opts);

    /*
        Get a config value.

        Please see the <hyprlang.hpp> header or https://hypr.land/hyprlang/ for docs regarding Hyprlang types.

        returns: a pointer to the config value struct, which is guaranteed to be valid for the life of this plugin, unless another `addConfigValue` is called afterwards.
                nullptr on error.
    */
    APICALL Hyprlang::CConfigValue* getConfigValue(HANDLE handle, const std::string& name);

    /*
        Register a dynamic (function) callback to a selected event.
        Pointer will be free'd by Hyprland on unregisterCallback().

        returns: a pointer to the newly allocated function. nullptr on fail.

        WARNING: Losing this pointer will unregister the callback!
    */
    APICALL [[nodiscard]] SP<HOOK_CALLBACK_FN> registerCallbackDynamic(HANDLE handle, const std::string& event, HOOK_CALLBACK_FN fn);

    /*
        Unregisters a callback. If the callback was dynamic, frees the memory.

        returns: true on success, false on fail

        Deprecated: just reset the pointer you received with registerCallbackDynamic
    */
    APICALL [[deprecated]] bool unregisterCallback(HANDLE handle, SP<HOOK_CALLBACK_FN> fn);

    /*
        Calls a hyprctl command.

        returns: the output (as in hyprctl)
    */
    APICALL std::string invokeHyprctlCommand(const std::string& call, const std::string& args, const std::string& format = "");

    /*
        Adds a layout to Hyprland.

        returns: true on success. False otherwise.
    */
    APICALL bool addLayout(HANDLE handle, const std::string& name, IHyprLayout* layout);

    /*
        Removes an added layout from Hyprland.

        returns: true on success. False otherwise.
    */
    APICALL bool removeLayout(HANDLE handle, IHyprLayout* layout);

    /*
        Queues a config reload. Does not take effect immediately.

        returns: true on success. False otherwise.
    */
    APICALL bool reloadConfig();

    /*
        Adds a notification.

        returns: true on success. False otherwise.
    */
    APICALL bool addNotification(HANDLE handle, const std::string& text, const CHyprColor& color, const float timeMs);

    /*
        Creates a trampoline function hook to an internal hl func.

        returns: CFunctionHook*

        !WARNING! Hooks are *not* guaranteed any API stability. Internal methods may be removed, added, or renamed. Consider preferring the API whenever possible.
    */
    APICALL CFunctionHook* createFunctionHook(HANDLE handle, const void* source, const void* destination);

    /*
        Removes a trampoline function hook. Will unhook if still hooked.

        returns: true on success. False otherwise.

        !WARNING! Hooks are *not* guaranteed any API stability. Internal methods may be removed, added, or renamed. Consider preferring the API whenever possible.
    */
    APICALL bool removeFunctionHook(HANDLE handle, CFunctionHook* hook);

    /*
        Gets a function address from a signature.
        This is useful for hooking private functions.

        returns: function address, or nullptr on fail.

        Deprecated because of findFunctionsByName.
    */
    APICALL [[deprecated]] void* getFunctionAddressFromSignature(HANDLE handle, const std::string& sig);

    /*
        Adds a window decoration to a window

        returns: true on success. False otherwise.
    */
    APICALL bool addWindowDecoration(HANDLE handle, PHLWINDOW pWindow, UP<IHyprWindowDecoration> pDecoration);

    /*
        Removes a window decoration

        returns: true on success. False otherwise.
    */
    APICALL bool removeWindowDecoration(HANDLE handle, IHyprWindowDecoration* pDecoration);

    /*
        Adds a keybind dispatcher.

        returns: true on success. False otherwise.

        DEPRECATED: use addDispatcherV2
    */
    APICALL [[deprecated]] bool addDispatcher(HANDLE handle, const std::string& name, std::function<void(std::string)> handler);

    /*
        Adds a keybind dispatcher.

        returns: true on success. False otherwise.
    */
    APICALL bool addDispatcherV2(HANDLE handle, const std::string& name, std::function<SDispatchResult(std::string)> handler);

    /*
        Removes a keybind dispatcher.

        returns: true on success. False otherwise.
    */
    APICALL bool removeDispatcher(HANDLE handle, const std::string& name);

    /*
        Adds a notification.

        data has to contain:
         - text: std::string or const char*
         - time: uint64_t
         - color: CHyprColor -> CHyprColor(0) will apply the default color for the notification icon

        data may contain:
         - icon: eIcons

        returns: true on success. False otherwise.
    */
    APICALL bool addNotificationV2(HANDLE handle, const std::unordered_map<std::string, std::any>& data);

    /*
        Returns a vector of found functions matching the provided name.

        These addresses will not change, and should be made static. Lookups are slow.

        Empty means either none found or handle was invalid
    */
    APICALL std::vector<SFunctionMatch> findFunctionsByName(HANDLE handle, const std::string& name);

    /*
        Returns the hyprland version data. It's highly advised to not run plugins compiled
        for a different hash.
    */
    APICALL SVersionInfo getHyprlandVersion(HANDLE handle);

    /*
        Registers a hyprctl command

        returns: Pointer. Nullptr on fail.
    */
    APICALL SP<SHyprCtlCommand> registerHyprCtlCommand(HANDLE handle, SHyprCtlCommand cmd);

    /*
        Unregisters a hyprctl command

        returns: true on success. False otherwise.
    */
    APICALL bool unregisterHyprCtlCommand(HANDLE handle, SP<SHyprCtlCommand> cmd);
};

// NOLINTBEGIN
/*
    Get the descriptive string this plugin/server was compiled with.

    This function will end up in both hyprland and any/all plugins,
    and can be found by a simple dlsym()

    _get_hash() is server,
    _get_client_hash() is client.
*/
APICALL EXPORT const char*        __hyprland_api_get_hash();
APICALL inline EXPORT const char* __hyprland_api_get_client_hash() {
    static auto stripPatch = [](const char* ver) -> std::string {
        std::string_view v = ver;
        if (!v.contains('.'))
            return std::string{v};

        return std::string{v.substr(0, v.find_last_of('.'))};
    };

    static const std::string ver = (std::string{GIT_COMMIT_HASH} + "_aq_" + stripPatch(AQUAMARINE_VERSION) + "_hu_" + stripPatch(HYPRUTILS_VERSION) + "_hg_" +
                                    stripPatch(HYPRGRAPHICS_VERSION) + "_hc_" + stripPatch(HYPRCURSOR_VERSION) + "_hlg_" + stripPatch(HYPRLANG_VERSION));

    return ver.c_str();
}
// NOLINTEND

#ifdef __clang__
#pragma clang diagnostic pop
#endif
