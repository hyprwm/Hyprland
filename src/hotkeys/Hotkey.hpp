#pragma once

#include "../defines.hpp"
#include "../helpers/signal/Signal.hpp"
#include "../input/Keys.hpp"

#include <optional>
#include <string>

class IHID;
class IKeyboard;

namespace Hotkeys {

    enum class eTriggerType : uint8_t {
        KEY = 0,
        BUTTON,
    };

    enum class eDenyReason : uint8_t {
        ALREADY_BOUND = 0,
        NOT_PERMITTED,
    };

    struct STrigger {
        eTriggerType        type = eTriggerType::KEY;
        uint32_t            code = 0; // keysym or button code
        Input::ModifierMask mods = Input::HL_MODIFIER_NONE;

        bool                operator==(const STrigger&) const = default;

        bool                isTap() const;
        bool                permitted() const;
        std::string         label() const;
    };

    struct SBindResult {
        std::optional<eDenyReason> denied;
        std::string                message;
    };

    // one global hotkey. Owned by the protocol object that requested it, tracked weakly by the manager.
    class CHotkey {
      public:
        SBindResult        commit(const STrigger& trigger, const std::string& appId, const std::string& description);

        bool               bound() const;
        const STrigger&    trigger() const;
        const std::string& appId() const;
        const std::string& description() const;

        struct {
            CSignalT<uint32_t, uint32_t> triggered; // serial, timeMs
            CSignalT<uint32_t, uint32_t> released;  // serial, timeMs
            CSignalT<std::string>        revoked;   // message
        } m_events;

      private:
        bool        onKey(uint32_t keycode, SP<IKeyboard> keyboard, bool pressed, Input::ModifierMask mods, uint32_t timeMs, bool locked);
        bool        onButton(uint32_t button, SP<IHID> pointer, bool pressed, Input::ModifierMask mods, uint32_t timeMs, bool locked);
        bool        press(uint32_t code, WP<IHID> device, uint32_t timeMs);
        bool        release(uint32_t code, const WP<IHID>& device, uint32_t timeMs);
        void        cancelTap(uint32_t exceptCode, const WP<IHID>& exceptDevice);
        void        resetHeld();
        void        revoke(const std::string& message);

        STrigger    m_trigger;
        std::string m_appId;
        std::string m_description;
        bool        m_bound = false;

        bool        m_held       = false;
        bool        m_tapPending = false;
        uint32_t    m_heldCode   = 0;
        WP<IHID>    m_heldDevice;

        friend class CHotkeyManager;
    };
};
