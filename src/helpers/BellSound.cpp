#include "BellSound.hpp"
#include <canberra.h>
#include "../config/ConfigValue.hpp"
#include "../config/ConfigManager.hpp"
#include "MiscFunctions.hpp"
#include "../event/EventBus.hpp"
#include "../debug/log/Logger.hpp"

CBellSound::CBellSound() {
    onNewConfig();
    static auto configListener = Event::bus()->m_events.config.reloaded.listen([this] { onNewConfig(); });
}

CBellSound::~CBellSound() {
    if (m_context) {
        ca_context_destroy(m_context);
        ca_proplist_destroy(m_sound);
    }
}

void CBellSound::onNewConfig() {
    const auto VALUE = *CConfigValue<std::string>("misc:bell_sound");

    if (VALUE == "default") {
        m_muted = false;
        initializeSoundContext();
        ca_proplist_sets(m_sound, CA_PROP_EVENT_ID, "bell-window-system");
    } else if (VALUE.empty() || VALUE == "none")
        m_muted = true;
    else {
        m_muted = false;
        initializeSoundContext();

        const auto RESOLVEDPATH = absolutePath(VALUE, Config::mgr()->getMainConfigPath());
        ca_proplist_sets(m_sound, CA_PROP_MEDIA_FILENAME, RESOLVEDPATH.c_str());
        ca_proplist_set(m_sound, CA_PROP_EVENT_ID, nullptr, 0);
        int result = validateCustomSound();
        if (result == CA_SUCCESS)
            return;

        ca_proplist_sets(m_sound, CA_PROP_EVENT_ID, "bell-window-system");
        if (result == CA_ERROR_CORRUPT)
            Log::logger->log(Log::WARN, "system bell: resolved custom sound path '{}' is not a valid wav/ogg file, falling back to default", RESOLVEDPATH);
        else
            Log::logger->log(Log::WARN, "system bell: validation for resolved custom sound path '{}' failed: '{}', falling back to default", RESOLVEDPATH, ca_strerror(result));
    }
}

void CBellSound::initializeSoundContext() {
    if (m_context)
        return;

    int result = ca_context_create(&m_context) || ca_proplist_create(&m_sound);
    if UNLIKELY (result != CA_SUCCESS)
        Log::logger->log(Log::ERR, "system bell: failed to create canberra context: '{}'", ca_strerror(result));

    ca_context_change_props(m_context, CA_PROP_APPLICATION_NAME, "Hyprland", CA_PROP_MEDIA_NAME, "System Bell", CA_PROP_EVENT_DESCRIPTION, "Wayland system bell",
                            CA_PROP_MEDIA_ROLE, "event", CA_PROP_MEDIA_ICON_NAME, "preferences-system-notifications", CA_PROP_CANBERRA_CACHE_CONTROL, "permanent", nullptr);
}

int CBellSound::validateCustomSound() {
    ca_proplist_sets(m_sound, CA_PROP_CANBERRA_VOLUME, "-inf");
    int result = ca_context_play_full(m_context, 0, m_sound, nullptr, nullptr);
    ca_proplist_set(m_sound, CA_PROP_CANBERRA_VOLUME, nullptr, 0);
    return result;
}

void CBellSound::play() {
    static CBellSound instance;

    if (instance.m_muted)
        return;

    int result = ca_context_play_full(instance.m_context, 0, instance.m_sound, nullptr, nullptr);
    if (result != CA_SUCCESS)
        Log::logger->log(Log::WARN, "system bell: failed to play sound, '{}'", ca_strerror(result));
}
