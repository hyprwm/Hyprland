#pragma once

/*

Hyprland Plugin API.

Most documentation will be made with comments in this code, but more info can be also found on the wiki.

!WARNING!
The Hyprland API passes C++ objects over, so no ABI compatibility is guaranteed.
Make sure to compile your plugins with the same compiler as Hyprland, and ideally,
on the same machine.

See examples/examplePlugin for an example plugin

*/

#define HYPRLAND_API_VERSION "0.1"

#include "../helpers/Color.hpp"
#include "HookSystem.hpp"
#include "../SharedDefs.hpp"

#include <any>
#include <functional>
#include <string>

typedef std::function<void(void*, std::any)> HOOK_CALLBACK_FN;
typedef struct {
    std::string name;
    std::string description;
    std::string author;
    std::string version;
} PLUGIN_DESCRIPTION_INFO;

struct SFunctionMatch {
    void*       address = nullptr;
    std::string signature;
    std::string demangled;
};

#define APICALL extern "C"
#define EXPORT  __attribute__((visibility("default")))
#define REQUIRED
#define OPTIONAL
#define HANDLE void*

class IHyprLayout;
class CWindow;
class IHyprWindowDecoration;
struct SConfigValue;

/*
    These methods are for the plugin to implement
    Methods marked with REQUIRED are required.
*/

/* 
    called pre-plugin init.
    In case of a version mismatch, will eject the .so.

    This function should not be modified, see the example plugin.
*/
typedef REQUIRED std::string (*PPLUGIN_API_VERSION_FUNC)();
#define PLUGIN_API_VERSION          pluginAPIVersion
#define PLUGIN_API_VERSION_FUNC_STR "pluginAPIVersion"

/*
    called on plugin init. Passes a handle as the parameter, which the plugin should keep for identification later.
    The plugin should return a PLUGIN_DESCRIPTION_INFO struct with information about itself.

    Keep in mind this is executed synchronously, and as such any blocking calls to hyprland might hang. (e.g. system("hyprctl ..."))
*/
typedef REQUIRED PLUGIN_DESCRIPTION_INFO (*PPLUGIN_INIT_FUNC)(HANDLE);
#define PLUGIN_INIT          pluginInit
#define PLUGIN_INIT_FUNC_STR "pluginInit"

/*
    called on plugin unload, if that was a user action. If the plugin is being unloaded by an error,
    this will not be called.

    Hooks are unloaded after exit.
*/
typedef OPTIONAL void (*PPLUGIN_EXIT_FUNC)(void);
#define PLUGIN_EXIT          pluginExit
#define PLUGIN_EXIT_FUNC_STR "pluginExit"

/*
    End plugin methods
*/

namespace HyprlandAPI {

    /*
        Add a config value.
        All config values MUST be in the plugin: namespace
        This method may only be called in "pluginInit"

        After you have registered ALL of your config values, you may call `getConfigValue`

        returns: true on success, false on fail
    */
    APICALL bool addConfigValue(HANDLE handle, const std::string& name, const SConfigValue& value);

    /*
        Get a config value.

        returns: a pointer to the config value struct, which is guaranteed to be valid for the life of this plugin, unless another `addConfigValue` is called afterwards.
                nullptr on error.
    */
    APICALL SConfigValue* getConfigValue(HANDLE handle, const std::string& name);

    /*
        Register a static (pointer) callback to a selected event.
        Pointer must be kept valid until unregisterCallback() is called.

        returns: true on success, false on fail
    */
    APICALL bool registerCallbackStatic(HANDLE handle, const std::string& event, HOOK_CALLBACK_FN* fn);

    /*
        Register a dynamic (function) callback to a selected event.
        Pointer will be free'd by Hyprland on unregisterCallback().

        returns: a pointer to the newly allocated function. nullptr on fail.
    */
    APICALL HOOK_CALLBACK_FN* registerCallbackDynamic(HANDLE handle, const std::string& event, HOOK_CALLBACK_FN fn);

    /*
        Unregisters a callback. If the callback was dynamic, frees the memory.

        returns: true on success, false on fail
    */
    APICALL bool unregisterCallback(HANDLE handle, HOOK_CALLBACK_FN* fn);

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
    APICALL bool addNotification(HANDLE handle, const std::string& text, const CColor& color, const float timeMs);

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
    APICALL bool addWindowDecoration(HANDLE handle, CWindow* pWindow, IHyprWindowDecoration* pDecoration);

    /*
        Removes a window decoration

        returns: true on success. False otherwise.
    */
    APICALL bool removeWindowDecoration(HANDLE handle, IHyprWindowDecoration* pDecoration);

    /*
        Adds a keybind dispatcher.

        returns: true on success. False otherwise.
    */
    APICALL bool addDispatcher(HANDLE handle, const std::string& name, std::function<void(std::string)> handler);

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
         - color: CColor -> CColor(0) will apply the default color for the notification icon

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
};