#pragma once

#include <unordered_set>
#include <vector>
#include <string>
#include <expected>
#include <type_traits>
#include <functional>
#include <optional>

#include "Key.hpp"

#include "../devices/IHID.hpp"
#include "../helpers/memory/Memory.hpp"

namespace Keybinds {

    enum eBindFlags : uint16_t {
        BIND_FLAG_LOCKED              = (1 << 0),
        BIND_FLAG_RELEASE             = (1 << 1),
        BIND_FLAG_REPEAT              = (1 << 2),
        BIND_FLAG_LONG_PRESS          = (1 << 3),
        BIND_FLAG_NON_CONSUMING       = (1 << 4),
        BIND_FLAG_AUTO_CONSUMING      = (1 << 5),
        BIND_FLAG_TRANSPARENT         = (1 << 6),
        BIND_FLAG_IGNORE_MODS         = (1 << 7),
        BIND_FLAG_DONT_INHIBIT        = (1 << 8),
        BIND_FLAG_CLICK               = (1 << 9),
        BIND_FLAG_DRAG                = (1 << 10),
        BIND_FLAG_SUBMAP_UNIVERSAL    = (1 << 11),
        BIND_FLAG_ALLOW_INPUT_CAPTURE = (1 << 12),
        BIND_FLAG_DEVICE_INCLUSIVE    = (1 << 13),
        BIND_FLAG_CATCH_ALL           = (1 << 14),
        BIND_FLAG_MOUSE               = (1 << 15),
    };

    using BindFlags = std::underlying_type_t<eBindFlags>;

    struct SBindMetadata {
        std::string                displayKey;
        std::optional<std::string> description;
        std::string                handler;
        std::string                argument;
        std::string                submap;
        std::string                submapReset;
    };

    struct SExtraBindArgs {
        std::unordered_set<std::string> devices;
        SBindMetadata                   metadata;
    };

    enum eBindFollowUp : uint8_t {
        BIND_FOLLOW_UP_NONE = 0,
        BIND_FOLLOW_UP_TRIGGER_RELEASE,
    };

    struct SBindResult {
        bool          passEvent = false;
        bool          success   = true;
        std::string   error;
        eBindFollowUp followUp = BIND_FOLLOW_UP_NONE;
    };

    using BindCallback = std::function<SBindResult()>;

    struct SBindEventContext {
        std::span<const SResolvedKey>              heldKeys = {};
        std::optional<SResolvedKey>                trigger;
        std::underlying_type_t<eKeyboardModifiers> modifiersNow     = 0;
        std::underlying_type_t<eKeyboardModifiers> modifiersAtPress = 0;
        bool                                       pressed          = true;
        SP<IHID>                                   device           = nullptr;
        std::string                                submap           = {};
    };

    enum eBindMatch : uint8_t {
        BIND_MATCH_NONE = 0,
        BIND_MATCH_PARTIAL,
        BIND_MATCH_FULL,
    };

    class CBind {
      public:
        static std::expected<CBind, std::string> make(std::vector<std::string>&& keys, BindFlags flags, BindCallback&& callback, SExtraBindArgs&& args = {});

        CBind(CBind&&) noexcept                                        = default;
        CBind& operator=(CBind&&) noexcept                             = default;
        CBind(const CBind&)                                            = delete;
        CBind&                                 operator=(const CBind&) = delete;

        eBindMatch                             matches(const SBindEventContext& ctx = {}) const;
        bool                                   matchesContext(const SBindEventContext& ctx) const;
        SBindResult                            invoke() const;
        bool                                   enabled() const;
        void                                   setEnabled(bool x);
        bool                                   hasFlag(eBindFlags flag) const;
        BindFlags                              flags() const;
        ModifierMask                           modifierMask() const;
        size_t                                 chordSize() const;
        bool                                   containsKey(const SResolvedKey& key) const;
        bool                                   isFullyHeld(const SBindEventContext& ctx) const;
        bool                                   isSubChordOf(const CBind& other, const SBindEventContext& ctx) const;
        bool                                   isOrderedPrefixOf(const CBind& other, const SBindEventContext& ctx) const;
        std::span<const CKey>                  keys() const;
        std::span<const std::string>           keyNames() const;
        const std::unordered_set<std::string>& devices() const;
        const SBindMetadata&                   metadata() const;

      private:
        CBind() = default;

        bool                            matchesDevice(SP<IHID> dev) const;

        BindCallback                    m_callback;
        BindFlags                       m_flags   = 0;
        ModifierMask                    m_modmask = 0;
        std::vector<CKey>               m_keys;
        std::vector<std::string>        m_keyNames;
        std::unordered_set<std::string> m_devices;
        SBindMetadata                   m_metadata;
        bool                            m_enabled = true;
    };
};
