#pragma once

#include "../defines.hpp"
#include "vicinae-hotkey-v1.hpp"
#include "./WaylandProtocol.hpp"
#include "../helpers/signal/Signal.hpp"
#include "../hotkeys/Hotkey.hpp"

#include <vector>
#include <string>

// hotkeys are owned by the protocol, not this manager, so they outlive it
class CVicinaeHotkeyManager {
  public:
    explicit CVicinaeHotkeyManager(SP<CVicinaeHotkeyManagerV1> resource);

    bool good();

  private:
    SP<CVicinaeHotkeyManagerV1> m_resource;

    friend class CHotkeyProtocol;
};

class CVicinaeHotkey {
  public:
    CVicinaeHotkey(SP<CVicinaeHotkeyV1> resource, const Hotkeys::STrigger& trigger, const std::string& appId, const std::string& description);

    bool good();

  private:
    SP<CVicinaeHotkeyV1> m_resource;
    SP<Hotkeys::CHotkey> m_hotkey;

    struct {
        CHyprSignalListener triggered;
        CHyprSignalListener released;
        CHyprSignalListener revoked;
    } m_listeners;

    friend class CHotkeyProtocol;
};

class CHotkeyProtocol : public IWaylandProtocol {
  public:
    CHotkeyProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) override;

  private:
    void                                   onBind(SP<CVicinaeHotkeyManagerV1> mgr, uint32_t id, uint32_t keysym, uint32_t protoMods, const char* appid, const char* description);
    void                                   destroyManager(CVicinaeHotkeyManager* mgr);
    void                                   destroyHotkey(CVicinaeHotkey* hk);

    std::vector<SP<CVicinaeHotkeyManager>> m_managers;
    std::vector<SP<CVicinaeHotkey>>        m_hotkeys;

    friend class CVicinaeHotkeyManager;
    friend class CVicinaeHotkey;
};

namespace PROTO {
    inline UP<CHotkeyProtocol> hotkey;
};
