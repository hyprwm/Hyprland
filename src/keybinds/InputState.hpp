#pragma once

#include "Registry.hpp"

#include "../devices/IHID.hpp"
#include "../helpers/math/Math.hpp"

#include <optional>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace Keybinds {

    struct SPressedInput {
        SResolvedKey           key;
        ModifierMask           modifiersAtPress = 0;
        bool                   forwarded        = true;
        bool                   capturedAtPress  = false;
        uint32_t               actionCode       = 0;
        uint32_t               actionMouseCode  = 0;
        uint32_t               actionTimeMs     = 0;
        std::string            submapAtPress;
        Vector2D               positionAtPress;
        WP<IHID>               device;
        std::vector<WP<CBind>> releaseCallbacks;
        std::vector<WP<CBind>> deferredBinds;
        std::vector<WP<CBind>> suppressedBinds;
        std::vector<WP<CBind>> armedReleaseBinds;
    };

    struct SPendingRelease {
        SResolvedKey key;
        WP<IHID>     device;
        PBind        bind;
        uint32_t     actionCode      = 0;
        uint32_t     actionMouseCode = 0;
        uint32_t     actionTimeMs    = 0;
    };

    class CInputState {
      public:
        bool                           press(SPressedInput&& input);
        std::optional<SPressedInput>   release(const SResolvedKey& key, const SP<IHID>& device);
        void                           clear();
        void                           clearDevice(const SP<IHID>& device);
        bool                           setForwarded(const SResolvedKey& key, const SP<IHID>& device, bool forwarded);
        SPressedInput*                 find(const SResolvedKey& key, const SP<IHID>& device);
        const SPressedInput*           find(const SResolvedKey& key, const SP<IHID>& device) const;
        std::vector<SPendingRelease>   takeReleaseCallbacks(const SResolvedKey& key, const SP<IHID>& device);
        std::vector<SPendingRelease>   takeReleaseCallbacks(const PBind& bind);
        std::vector<SPendingRelease>   takeReleaseCallbacksForDevice(const SP<IHID>& device);
        std::vector<SPendingRelease>   takeAllReleaseCallbacks();

        bool                           isKeycodeDown(xkb_keycode_t keycode) const;
        bool                           isKeysymDown(xkb_keysym_t keysym) const;
        bool                           isEventDown(std::string_view event) const;

        std::span<SPressedInput>       pressed();
        std::span<const SPressedInput> pressed() const;
        std::span<const SResolvedKey>  heldKeys() const;

      private:
        std::optional<size_t>        indexOf(const SResolvedKey& key, const SP<IHID>& device) const;
        static bool                  sameIdentity(const SResolvedKey& lhs, const SResolvedKey& rhs);
        static bool                  equivalentHeldKey(const SResolvedKey& lhs, const SResolvedKey& rhs);
        std::vector<SPendingRelease> takeReleaseCallbacks(const std::function<bool(const SPressedInput&, const PBind&)>& predicate);
        void                         rebuildHeldKeys();

        std::vector<SPressedInput>   m_pressed;
        std::vector<SResolvedKey>    m_heldKeys;
    };
}
