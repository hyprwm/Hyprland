#pragma once

#include "../defines.hpp"
#include "vicinae-hotkey-v1.hpp"
#include "./WaylandProtocol.hpp"
#include "../helpers/signal/Signal.hpp"

#include <vector>
#include <string>
#include <xkbcommon/xkbcommon.h>

// hotkeys are owned by the protocol, not this manager, so they outlive it
class CVicinaeHotkeyManager {
  public:
    CVicinaeHotkeyManager(SP<CVicinaeHotkeyManagerV1> resource);

    bool good();

  private:
    SP<CVicinaeHotkeyManagerV1> m_resource;

    friend class CHotkeyProtocol;
};

class CHotkeyProtocol : public IWaylandProtocol {
  public:
    CHotkeyProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) override;

    // returns true if a bound hotkey consumed the key
    bool onKey(xkb_keysym_t keysym, uint32_t modmask, uint32_t keycode, bool pressed, uint32_t timeMs);

    void revokeConflicting();

  private:
    struct SBoundHotkey {
        SP<CVicinaeHotkeyV1> resource;
        xkb_keysym_t         keysym  = XKB_KEY_NoSymbol;
        uint32_t             modmask = 0; // HL_MODIFIER_* bits
        std::string          appid;
        std::string          description;
        bool                 bound    = false;
        bool                 held     = false;
        uint32_t             heldCode = 0;
    };

    void onBind(SP<CVicinaeHotkeyManagerV1> mgr, uint32_t id, xkb_keysym_t keysym, uint32_t protoMods, const char* appid, const char* description);
    void destroyManager(CVicinaeHotkeyManager* mgr);
    bool comboTakenByHotkey(xkb_keysym_t keysym, uint32_t modmask);

    std::vector<SP<CVicinaeHotkeyManager>> m_managers;
    std::vector<SP<SBoundHotkey>>          m_hotkeys;

    CHyprSignalListener                    m_reloadListener;

    friend class CVicinaeHotkeyManager;
};

namespace PROTO {
    inline UP<CHotkeyProtocol> hotkey;
};
