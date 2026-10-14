#include "Engine.hpp"

#include <hyprutils/i18n/I18nEngine.hpp>
#include "../config/ConfigValue.hpp"

using namespace I18n;
using namespace Hyprutils::I18n;

static SP<Hyprutils::I18n::CI18nEngine> huEngine;
static std::string                      localeStr;

//
SP<I18n::CI18nEngine> I18n::i18nEngine() {
    static SP<I18n::CI18nEngine> engine = makeShared<I18n::CI18nEngine>();
    return engine;
}

// prevents CI18nEngine constructor from being bloated by std::string allocations/deallocations
[[gnu::noinline]] static void registerEntry(const char* locale, eI18nKeys key, const char* translation) {
    huEngine->registerEntry(locale, key, translation);
}

[[gnu::noinline]] static void registerEntry(const char* locale, eI18nKeys key, const char* (*translationFunc)(const Hyprutils::I18n::translationVarMap&)) {
    huEngine->registerEntry(locale, key, translationFunc);
}

I18n::CI18nEngine::CI18nEngine() {
    huEngine = makeShared<Hyprutils::I18n::CI18nEngine>();
    huEngine->setFallbackLocale("en_US");
    localeStr = huEngine->getSystemLocale().locale();

    // en_US (English)
    registerEntry("en_US", TXT_KEY_ANR_TITLE, "Application Not Responding");
    registerEntry("en_US", TXT_KEY_ANR_CONTENT, "An application {title} - {class} is not responding.\nWhat do you want to do with it?");
    registerEntry("en_US", TXT_KEY_ANR_OPTION_TERMINATE, "Terminate");
    registerEntry("en_US", TXT_KEY_ANR_OPTION_WAIT, "Wait");
    registerEntry("en_US", TXT_KEY_ANR_PROP_UNKNOWN, "(unknown)");

    registerEntry("en_US", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "An application <b>{app}</b> is requesting an unknown permission.");
    registerEntry("en_US", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "An application <b>{app}</b> is trying to capture your screen.\n\nDo you want to allow it to?");
    registerEntry("en_US", TXT_KEY_PERMISSION_REQUEST_CURSOR_POS, "An application <b>{app}</b> is trying to capture your cursor position.\n\nDo you want to allow it to?");
    registerEntry("en_US", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "An application <b>{app}</b> is trying to load a plugin: <b>{plugin}</b>.\n\nDo you want to allow it to?");
    registerEntry("en_US", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "A new keyboard has been detected: <b>{keyboard}</b>.\n\nDo you want to allow it to operate?");
    huEngine->registerEntry("en_US", TXT_KEY_PERMISSION_REQUEST_INPUT_CAPTURE,
                            "An application <b>{app}</b> is trying to capture your inputs.\n\nDo you want to allow it to do so?");
    registerEntry("en_US", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(unknown)");
    registerEntry("en_US", TXT_KEY_PERMISSION_TITLE, "Permission request");
    registerEntry("en_US", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Hint: you can set persistent rules for these in the Hyprland config file.");
    registerEntry("en_US", TXT_KEY_PERMISSION_ALLOW, "Allow");
    registerEntry("en_US", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Allow and remember");
    registerEntry("en_US", TXT_KEY_PERMISSION_ALLOW_ONCE, "Allow once");
    registerEntry("en_US", TXT_KEY_PERMISSION_DENY, "Deny");
    registerEntry("en_US", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Unknown application (wayland client ID {wayland_id})");

    registerEntry("en_US", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                  "Your XDG_CURRENT_DESKTOP environment seems to be managed externally, and the current value is {value}.\nThis might cause issues unless it's intentional.");
    registerEntry("en_US", TXT_KEY_NOTIF_NO_GUIUTILS,
                  "Your system does not have hyprland-guiutils installed. This is a runtime dependency for some dialogs. Consider installing it.");
    registerEntry("en_US", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "Hyprland failed to load {count} essential asset, blame your distro's packager for doing a bad job at packaging!";
        return "Hyprland failed to load {count} essential assets, blame your distro's packager for doing a bad job at packaging!";
    });
    registerEntry("en_US", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                  "Your monitor layout is set up incorrectly. Monitor {name} overlaps with other monitor(s) in the layout.\nPlease see the wiki (Monitors page) for "
                  "more. This <b>will</b> cause issues.");
    registerEntry("en_US", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "Monitor {name} failed to set any requested modes, falling back to mode {mode}.");
    registerEntry("en_US", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "Invalid scale passed to monitor {name}: {scale}, using suggested scale: {fixed_scale}");
    registerEntry("en_US", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "Failed to load plugin {name}: {error}");
    registerEntry("en_US", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "CM shader reload failed, falling back to rgba/rgbx.");
    registerEntry("en_US", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Monitor {name}: wide color gamut is enabled but the display is not in 10-bit mode.");
    registerEntry("en_US", TXT_KEY_NOTIF_NO_WATCHDOG, "Hyprland was started without start-hyprland. This is strongly discouraged unless you are in a debugging environment.");
    registerEntry("en_US", TXT_KEY_NOTIF_DEPRECATED_CONFIG_OPTS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "You are using {count} deprecated config option. Run hyprctl deprecated-config to see more information.";
        return "You are using {count} deprecated config options. Run hyprctl deprecated-config to see more information.";
    });
    registerEntry("en_US", TXT_KEY_NOTIF_NO_FP16, "Your GPU does not support rendering to FP16 buffers, some effects and CM settings might be unavailable.");

    registerEntry("en_US", TXT_KEY_SAFE_MODE_TITLE, "Safe Mode");
    registerEntry("en_US", TXT_KEY_SAFE_MODE_DESCRIPTION,
                  "Hyprland has been launched in safe mode, which means your last session crashed.\nSafe mode prevents your config from being loaded. You can "
                  "troubleshoot in this environment, or load your config with the button below.\nDefault keybinds apply: SUPER+Q for kitty, SUPER+R for a basic runner, "
                  "SUPER+M to exit.\nRestarting "
                  "Hyprland will launch in normal mode again.");
    registerEntry("en_US", TXT_KEY_SAFE_MODE_BUTTON_LOAD_CONFIG, "Load config");
    registerEntry("en_US", TXT_KEY_SAFE_MODE_BUTTON_OPEN_CRASH_REPORT_DIR, "Open crash report directory");
    registerEntry("en_US", TXT_KEY_SAFE_MODE_BUTTON_UNDERSTOOD, "Ok, close this");
}

std::string I18n::CI18nEngine::localize(eI18nKeys key, const Hyprutils::I18n::translationVarMap& vars) {
    static auto CONFIG_LOCALE = CConfigValue<std::string>("general:locale");
    std::string locale        = *CONFIG_LOCALE != "" ? *CONFIG_LOCALE : localeStr;
    return huEngine->localizeEntry(locale, key, vars);
}
