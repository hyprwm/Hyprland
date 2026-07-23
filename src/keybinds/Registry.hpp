#pragma once

#include "Bind.hpp"

#include <span>
#include <string_view>
#include <vector>

namespace Keybinds {

    using PBind = SP<CBind>;

    class CRegistry {
      public:
        PBind                  add(CBind&& bind);
        bool                   remove(const PBind& bind);
        size_t                 removeByDisplayKey(std::string_view displayKey);
        std::vector<PBind>     findByDisplayKey(std::string_view displayKey) const;
        void                   clear();

        std::span<const PBind> binds() const;
        bool                   contains(const PBind& bind) const;
        bool                   empty() const;
        size_t                 size() const;
        bool                   hasSubmap(std::string_view submap) const;
        PBind                  findShortcutConflict(xkb_keysym_t keysym, ModifierMask modifiers, xkb_state* xkbState = nullptr) const;

      private:
        static std::string normalizeDisplayKey(std::string_view displayKey);

        std::vector<PBind> m_binds;
    };
}
