#pragma once

#include "InputState.hpp"
#include "Registry.hpp"

#include "../devices/IPointer.hpp"
#include "../helpers/time/Timer.hpp"
#include "../managers/eventLoop/EventLoopTimer.hpp"

#include <any>
#include <unordered_set>

class IKeyboard;

namespace Keybinds {

    class CKeybindManager {
      public:
        CKeybindManager();
        ~CKeybindManager();

        PBind              addBind(CBind&& bind);
        bool               removeBind(const PBind& bind);
        size_t             removeBinds(std::string_view displayKey);
        void               clearBinds();

        bool               onKeyEvent(std::any event, SP<IKeyboard> keyboard);
        bool               onAxisEvent(const IPointer::SAxisEvent& event, SP<IPointer> pointer);
        bool               onMouseEvent(const IPointer::SButtonEvent& event, SP<IPointer> pointer, bool captured = false);
        void               onSwitchEvent(const std::string& switchName);
        void               onSwitchOnEvent(const std::string& switchName);
        void               onSwitchOffEvent(const std::string& switchName);
        void               onDeviceRemoved(const SP<IHID>& device);

        void               updateXKBTranslationState();
        PBind              findConflictingBind(xkb_keysym_t keysym, ModifierMask modifiers) const;
        void               shadowBinds(const std::optional<SResolvedKey>& excluded = std::nullopt, const SP<IHID>& excludedDevice = nullptr);

        CRegistry&         registry();
        const CRegistry&   registry() const;
        CInputState&       inputState();
        const CInputState& inputState() const;
        std::string_view   currentSubmap() const;

      private:
        struct STimedBatch {
            std::vector<WP<CBind>> binds;
            SResolvedKey           trigger;
            WP<IHID>               device;
            WP<IKeyboard>          keyboard;
            bool                   hasDevice         = false;
            bool                   requiresHeldInput = false;

            bool                   empty() const;
            void                   clear();
        };

        SBindResult                   processEvent(const SBindEventContext& context, const SP<IKeyboard>& keyboard, SPressedInput* pressedInput = nullptr);
        SBindResult                   invokeBind(const PBind& bind, bool pressed, SPressedInput* pressedInput = nullptr);
        void                          invokeReleaseCallbacks(std::vector<SPendingRelease>&& releases);
        void                          invokeDeferredBinds(const SResolvedKey& key, const SP<IHID>& device, const SP<IKeyboard>& keyboard = nullptr);
        void                          suppressSubChords(const PBind& completed, const SBindEventContext& context);
        bool                          isSuppressed(const SPressedInput& input, const PBind& bind) const;
        void                          cancelTimedBinds();
        void                          scheduleLongPress(const std::vector<PBind>& binds, const SBindEventContext& context, const SP<IKeyboard>& keyboard, bool requiresHeldInput);
        void                          scheduleRepeat(const std::vector<PBind>& binds, const SBindEventContext& context, const SP<IKeyboard>& keyboard, bool requiresHeldInput);
        bool                          handleInternalKeybinds(xkb_keysym_t keysym);
        bool                          handleVT(xkb_keysym_t keysym);
        bool                          canInvokeNow(const PBind& bind) const;
        uint32_t                      keycodeToModifier(xkb_keycode_t keycode) const;

        CRegistry                     m_registry;
        CInputState                   m_inputState;
        std::unordered_set<WP<CBind>> m_shadowed;

        SP<CEventLoopTimer>           m_longPressTimer;
        SP<CEventLoopTimer>           m_repeatTimer;
        STimedBatch                   m_longPress;
        STimedBatch                   m_repeat;
        uint32_t                      m_repeatRate = 50;
        CTimer                        m_scrollTimer;
        bool                          m_timersRegistered = false;
        uint64_t                      m_repeatGeneration = 0;

        xkb_state*                    m_xkbTranslationState = nullptr;
    };

    UP<CKeybindManager>& mgr();
}
