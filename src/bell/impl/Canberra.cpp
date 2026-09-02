#include "Canberra.hpp"

#include "../../macros.hpp"
#include "../../config/ConfigValue.hpp"
#include "../../config/ConfigManager.hpp"
#include "../../helpers/MiscFunctions.hpp"

extern "C" {
#include <canberra.h>
}

using namespace Bell;

CCanberraImpl::CCanberraImpl() : m_thread([this](const std::string& data) { playThreaded(data); }) {
    ;
}

CCanberraImpl::~CCanberraImpl() {
    m_thread.stop();

    if (m_context) {
        ca_context_destroy(m_context);
        ca_proplist_destroy(m_sound);
    }
}

void CCanberraImpl::playThreaded(const std::string& x) {
    // This is ran on another thread.

    initializeSoundContext();

    if (x.empty() || x == "none")
        return;

    if (x == "default")
        ca_proplist_sets(m_sound, CA_PROP_EVENT_ID, "bell-window-system");
    else {
        const auto RESOLVEDPATH = absolutePath(x, Config::mgr()->getMainConfigPath());

        if (std::filesystem::exists(RESOLVEDPATH)) {
            ca_proplist_sets(m_sound, CA_PROP_MEDIA_FILENAME, RESOLVEDPATH.c_str());
            ca_proplist_set(m_sound, CA_PROP_EVENT_ID, nullptr, 0);
            LOG(Log::WARN, "CCanberraImpl::playThreaded: resolved custom sound path '{}'", RESOLVEDPATH);
        } else {
            ca_proplist_sets(m_sound, CA_PROP_EVENT_ID, "bell-window-system");
            LOG(Log::WARN, "CCanberraImpl::playThreaded: resolved custom sound path '{}' doesn't exist, falling back to default", RESOLVEDPATH);
        }
    }

    int result = ca_context_play_full(m_context, 0, m_sound, nullptr, nullptr);
    if UNLIKELY (result != CA_SUCCESS) {
        if (result == CA_ERROR_CORRUPT)
            LOG(Log::WARN, "bell: sound is not a wav/ogg file");
        else
            LOG(Log::WARN, "bell: failed to play sound, '{}'", ca_strerror(result));
    }
}

void CCanberraImpl::play() const {
    const auto BELLSOUND = CConfigValue<std::string>("misc:bell_sound");

    // TODO: make a config watcher possible raaah fucker

    m_thread.queue(*BELLSOUND);
}

void CCanberraImpl::initializeSoundContext() {
    if (m_context)
        return;

    int result = ca_context_create(&m_context) || ca_proplist_create(&m_sound);
    if UNLIKELY (result != CA_SUCCESS)
        LOG(Log::ERR, "bell: failed to create canberra context, '{}'", ca_strerror(result));

    ca_context_change_props(m_context, CA_PROP_APPLICATION_NAME, "Hyprland", CA_PROP_MEDIA_NAME, "System Bell", CA_PROP_EVENT_DESCRIPTION, "Wayland system bell",
                            CA_PROP_MEDIA_ROLE, "event", CA_PROP_MEDIA_ICON_NAME, "preferences-system-notifications", CA_PROP_CANBERRA_CACHE_CONTROL, "permanent", nullptr);
}
