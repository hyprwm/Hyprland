#pragma once

#include "../defines.hpp"
#include "xx-hotkey-v1.hpp"
#include "./WaylandProtocol.hpp"
#include "../helpers/signal/Signal.hpp"
#include "../hotkeys/Hotkey.hpp"

#include <optional>
#include <string>
#include <vector>

class CXXHotkeyManagerResource {
  public:
    explicit CXXHotkeyManagerResource(SP<CXxHotkeyManagerV1> resource);

    bool good();

  private:
    SP<CXxHotkeyManagerV1>       m_resource;
    std::string                  m_appId;
    bool                         m_appIdSet  = false;
    bool                         m_committed = false;
    WP<CXXHotkeyManagerResource> m_self;

    friend class CXXHotkeyProtocol;
    friend class CXXHotkeyResource;
};

// hotkeys are owned by the protocol, not their manager, so they outlive it
class CXXHotkeyResource {
  public:
    CXXHotkeyResource(SP<CXxHotkeyV1> resource, WP<CXXHotkeyManagerResource> manager);

    bool good();

  private:
    void                             commit();

    SP<CXxHotkeyV1>                  m_resource;
    WP<CXXHotkeyManagerResource>     m_manager;
    SP<Hotkeys::CHotkey>             m_hotkey;

    std::optional<Hotkeys::STrigger> m_pendingTrigger;
    std::string                      m_pendingDescription;

    struct {
        CHyprSignalListener triggered;
        CHyprSignalListener released;
        CHyprSignalListener revoked;
    } m_listeners;

    friend class CXXHotkeyProtocol;
};

class CXXHotkeyProtocol : public IWaylandProtocol {
  public:
    CXXHotkeyProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) override;

  private:
    void                                      createHotkey(WP<CXXHotkeyManagerResource> mgr, uint32_t id);
    void                                      destroyManager(CXXHotkeyManagerResource* mgr);
    void                                      destroyHotkey(CXXHotkeyResource* hk);

    std::vector<SP<CXXHotkeyManagerResource>> m_managers;
    std::vector<SP<CXXHotkeyResource>>        m_hotkeys;

    friend class CXXHotkeyManagerResource;
    friend class CXXHotkeyResource;
};

namespace PROTO {
    inline UP<CXXHotkeyProtocol> xxHotkey;
};
