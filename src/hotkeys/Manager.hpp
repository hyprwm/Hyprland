#pragma once

#include "Hotkey.hpp"
#include "../devices/IKeyboard.hpp"
#include "../devices/IPointer.hpp"

#include <vector>

namespace Hotkeys {

    // shared backend for the client-managed global hotkey protocols (vicinae-hotkey-v1, xx-hotkey-v1)
    class CHotkeyManager {
      public:
        CHotkeyManager();

        SP<CHotkey> create();

        // return true if a bound hotkey consumed the event
        bool                     onKey(const IKeyboard::SKeyEvent& event, SP<IKeyboard> keyboard);
        bool                     onButton(const IPointer::SButtonEvent& event, SP<IPointer> pointer);
        void                     onAxis();

        std::vector<SP<CHotkey>> bound();

      private:
        SBindResult              check(const CHotkey& self, const STrigger& trigger);
        void                     revokeConflicting();
        void                     cancelTaps(uint32_t exceptCode = 0, const WP<IHID>& exceptDevice = {});
        void                     prune();

        std::vector<WP<CHotkey>> m_hotkeys;
        CHyprSignalListener      m_reloadListener;

        friend class CHotkey;
    };

    // both protocols define the same shift / ctrl / alt / super bitfield
    Input::ModifierMask modsFromProtocol(uint32_t mods);
    bool                validProtocolMods(uint32_t mods);

    UP<CHotkeyManager>& mgr();
};
