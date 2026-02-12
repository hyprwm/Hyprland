#pragma once

#include "../helpers/memory/Memory.hpp"
#include <hyprutils/i18n/I18nEngine.hpp>
#include <unordered_map>
#include <cstdint>
#include <string>
#include <functional>

namespace I18n {

    enum eI18nKeys : uint8_t {
        TXT_KEY_ANR_TITLE = 0,
        TXT_KEY_ANR_CONTENT,
        TXT_KEY_ANR_OPTION_TERMINATE,
        TXT_KEY_ANR_OPTION_WAIT,
        TXT_KEY_ANR_PROP_UNKNOWN,

        TXT_KEY_PERMISSION_REQUEST_UNKNOWN,
        TXT_KEY_PERMISSION_REQUEST_SCREENCOPY,
        TXT_KEY_PERMISSION_REQUEST_PLUGIN,
        TXT_KEY_PERMISSION_REQUEST_KEYBOARD,
        TXT_KEY_PERMISSION_UNKNOWN_NAME,
        TXT_KEY_PERMISSION_TITLE,
        TXT_KEY_PERMISSION_PERSISTENCE_HINT,
        TXT_KEY_PERMISSION_ALLOW,
        TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER,
        TXT_KEY_PERMISSION_ALLOW_ONCE,
        TXT_KEY_PERMISSION_DENY,
        TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP,

        TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
        TXT_KEY_NOTIF_NO_GUIUTILS,
        TXT_KEY_NOTIF_FAILED_ASSETS,
        TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
        TXT_KEY_NOTIF_MONITOR_MODE_FAIL,
        TXT_KEY_NOTIF_MONITOR_AUTO_SCALE,
        TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN,
        TXT_KEY_NOTIF_CM_RELOAD_FAILED,
        TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B,
        TXT_KEY_NOTIF_NO_WATCHDOG,

        TXT_KEY_SAFE_MODE_TITLE,
        TXT_KEY_SAFE_MODE_DESCRIPTION,
        TXT_KEY_SAFE_MODE_BUTTON_OPEN_CRASH_REPORT_DIR,
        TXT_KEY_SAFE_MODE_BUTTON_LOAD_CONFIG,
        TXT_KEY_SAFE_MODE_BUTTON_UNDERSTOOD,
    };

    using TranslationFn = std::function<std::string(const Hyprutils::I18n::translationVarMap&)>;

    class CI18nEngine {
      public:
        CI18nEngine();
        ~CI18nEngine() = default;
        void        registerEntry(const std::string& locale, const std::string& key, TranslationFn fn);
        std::string localize(eI18nKeys key, const Hyprutils::I18n::translationVarMap& vars = {}) const;

      private:
        std::unordered_map<std::string, std::unordered_map<std::string, TranslationFn> > m_entries;
    };

    SP<CI18nEngine> i18nEngine();
};
