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

#define APICALL extern "C"
#define EXPORT  __attribute__((visibility("default")))
#define REQUIRED
#define OPTIONAL
#define HANDLE void*

/*
    These methods are for the plugin to implement
    Methods marked with REQUIRED are required.
*/

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

};