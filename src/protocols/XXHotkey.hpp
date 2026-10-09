#pragma once

#include "../defines.hpp"
#include "xx-hotkey-v1.hpp"
#include "./WaylandProtocol.hpp"
#include "../devices/IKeyboard.hpp"
#include "../devices/IPointer.hpp"
#include "../input/Keys.hpp"

#include <optional>
#include <string>
#include <vector>
#include <xkbcommon/xkbcommon.h>

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
    enum class eTriggerType : uint8_t {
        TRIGGER_KEY = 0,
        TRIGGER_BUTTON,
    };

    struct STrigger {
        eTriggerType        type = eTriggerType::TRIGGER_KEY;
        uint32_t            code = 0; // keysym or button code
        Input::ModifierMask mods = Input::HL_MODIFIER_NONE;
    };

    static bool                  acceptable(const STrigger& trigger);
    void                         commit();
    bool                         isTap() const;
    void                         resetHeld();

    SP<CXxHotkeyV1>              m_resource;
    WP<CXXHotkeyManagerResource> m_manager;

    std::optional<STrigger>      m_pendingTrigger;
    std::string                  m_pendingDescription;

    STrigger                     m_trigger;
    std::string                  m_description;
    std::string                  m_appId;
    bool                         m_bound = false;

    bool                         m_held       = false;
    bool                         m_tapPending = false;
    uint32_t                     m_heldCode   = 0;
    WP<IHID>                     m_heldDevice;

    friend class CXXHotkeyProtocol;
};

class CXXHotkeyProtocol : public IWaylandProtocol {
  public:
    struct SHotkeyInfo {
        std::string appId;
        std::string trigger;
        std::string description;
    };

    CXXHotkeyProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) override;

    // return true if a bound hotkey consumed the event
    bool                     onKey(const IKeyboard::SKeyEvent& event, SP<IKeyboard> keyboard);
    bool                     onButton(const IPointer::SButtonEvent& event, SP<IPointer> pointer);
    void                     onAxis();

    std::vector<SHotkeyInfo> getAllShortcuts();

  private:
    void                                      createHotkey(WP<CXXHotkeyManagerResource> mgr, uint32_t id);
    void                                      destroyManager(CXXHotkeyManagerResource* mgr);
    void                                      destroyHotkey(CXXHotkeyResource* hk);
    void                                      cancelTaps(uint32_t exceptCode = 0, const WP<IHID>& exceptDevice = {});

    std::vector<SP<CXXHotkeyManagerResource>> m_managers;
    std::vector<SP<CXXHotkeyResource>>        m_hotkeys;

    friend class CXXHotkeyManagerResource;
    friend class CXXHotkeyResource;
};

namespace PROTO {
    inline UP<CXXHotkeyProtocol> xxHotkey;
};
