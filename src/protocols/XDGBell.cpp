#include "XDGBell.hpp"
#include "core/Compositor.hpp"
#include "../desktop/view/Window.hpp"
#include "../managers/EventManager.hpp"
#include "../Compositor.hpp"

#include "../config/ConfigManager.hpp"
#include "../config/ConfigValue.hpp"
#include "../helpers/MiscFunctions.hpp"
#include "../debug/log/Logger.hpp"

#include <canberra.h>

#include <filesystem>
#include <format>
#include <string>

namespace {
    enum class eBellSoundMode : uint8_t {
        DEFAULT,
        NONE,
        FILE,
    };

    struct SBellSoundConfig {
        eBellSoundMode mode = eBellSoundMode::DEFAULT;
        std::string    filePath;
    };

    SBellSoundConfig parseBellSoundConfigValue(const std::string& value) {
        if (value.empty() || value == "default")
            return {.mode = eBellSoundMode::DEFAULT};

        if (value == "none")
            return {.mode = eBellSoundMode::NONE};

        const auto resolvedPath = absolutePath(value, Config::mgr()->getMainConfigPath());

        std::error_code ec;
        if (!std::filesystem::exists(resolvedPath, ec) || ec || !std::filesystem::is_regular_file(resolvedPath, ec) || ec) {
            Log::logger->log(Log::WARN, "bell: configured custom sound path '{}' is invalid, falling back to default bell sound", resolvedPath);
            return {.mode = eBellSoundMode::DEFAULT};
        }

        return {
            .mode     = eBellSoundMode::FILE,
            .filePath = resolvedPath,
        };
    }

    SBellSoundConfig getBellSoundConfig() {
        static auto PBELLSOUND = CConfigValue<std::string>("misc:bell_sound");
        return parseBellSoundConfigValue(*PBELLSOUND);
    }

    std::string getBellEventData(wl_resource* surface) {
        if (!surface)
            return "";

        const auto SURFACE = CWLSurfaceResource::fromResource(surface);
        if (!SURFACE)
            return "";

        for (const auto& w : g_pCompositor->m_windows) {
            if (!w->m_isMapped || w->m_isX11 || !w->m_xdgSurface || !w->wlSurface())
                continue;

            if (w->wlSurface()->resource() == SURFACE)
                return std::format("{:x}", rc<uintptr_t>(w.get()));
        }

        return "";
    }

    void emitBellEvent(const std::string& data) {
        g_pEventManager->postEvent(SHyprIPCEvent{
            .event = "bell",
            .data  = data,
        });
    }

    class CBellAudioContext {
      public:
        ~CBellAudioContext() {
            if (m_context)
                ca_context_destroy(m_context);
        }

        ca_context* get() {
            if (m_attemptedInit)
                return m_context;

            m_attemptedInit = true;

            int result = ca_context_create(&m_context);
            if (result < 0) {
                Log::logger->log(Log::WARN, "bell: failed to create canberra context: {}", ca_strerror(result));
                return nullptr;
            }

            result = ca_context_change_props(
                m_context,
                CA_PROP_APPLICATION_NAME, "Hyprland",
                CA_PROP_APPLICATION_ID, "org.hyprland.Hyprland",
                nullptr
            );

            if (result < 0)
                Log::logger->log(Log::WARN, "bell: failed to set canberra context properties: {}", ca_strerror(result));

            result = ca_context_open(m_context);
            if (result < 0) {
                Log::logger->log(Log::WARN, "bell: failed to open canberra context: {}", ca_strerror(result));
                ca_context_destroy(m_context);
                m_context = nullptr;
                return nullptr;
            }

            return m_context;
        }

      private:
        bool        m_attemptedInit = false;
        ca_context* m_context       = nullptr;
    };

    CBellAudioContext& getAudioContext() {
        static CBellAudioContext context;
        return context;
    }

    void playBellSound(const SBellSoundConfig& config) {
        if (config.mode == eBellSoundMode::NONE)
            return;

        auto* const CONTEXT = getAudioContext().get();
        if (!CONTEXT)
            return;

        int result = 0;

        if (config.mode == eBellSoundMode::DEFAULT) {
            result = ca_context_play(
                CONTEXT,
                0,
                CA_PROP_EVENT_ID, "bell-window-system",
                CA_PROP_EVENT_DESCRIPTION, "Wayland system bell",
                nullptr
            );
        } else {
            result = ca_context_play(
                CONTEXT,
                0,
                CA_PROP_MEDIA_FILENAME, config.filePath.c_str(),
                CA_PROP_EVENT_DESCRIPTION, "Wayland system bell",
                nullptr
            );
        }

        if (result < 0)
            Log::logger->log(Log::WARN, "bell: failed to play sound: {}", ca_strerror(result));
    }

    void handleBell(wl_resource* surface) {
        const auto bellData = getBellEventData(surface);

        emitBellEvent(bellData);
        playBellSound(getBellSoundConfig());
    }
}

CXDGSystemBellManagerResource::CXDGSystemBellManagerResource(UP<CXdgSystemBellV1>&& resource) : m_resource(std::move(resource)) {
    if UNLIKELY (!good())
        return;

    m_resource->setDestroy([this](CXdgSystemBellV1* r) { PROTO::xdgBell->destroyResource(this); });
    m_resource->setOnDestroy([this](CXdgSystemBellV1* r) { PROTO::xdgBell->destroyResource(this); });

    m_resource->setRing([](CXdgSystemBellV1* r, wl_resource* surface) {
        handleBell(surface);
    });
}

bool CXDGSystemBellManagerResource::good() {
    return m_resource->resource();
}

CXDGSystemBellProtocol::CXDGSystemBellProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CXDGSystemBellProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = WP<CXDGSystemBellManagerResource>{
        m_managers.emplace_back(makeUnique<CXDGSystemBellManagerResource>(makeUnique<CXdgSystemBellV1>(client, ver, id)))
    };

    if UNLIKELY (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        return;
    }
}

void CXDGSystemBellProtocol::destroyResource(CXDGSystemBellManagerResource* res) {
    std::erase_if(m_managers, [&](const auto& other) { return other.get() == res; });
}
