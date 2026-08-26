#pragma once

#include "Bind.hpp"
#include "Submap.hpp"

#include <span>
#include <string_view>
#include <vector>

namespace Keybinds {

    using PBind   = SP<CBind>;
    using PSubmap = SP<CSubmap>;

    class CRegistry {
      public:
        PBind                    add(CBind&& bind);
        PSubmap                  addSubmap(CSubmap&& submap);
        bool                     remove(const PBind& bind);
        size_t                   removeByDisplayKey(std::string_view displayKey);
        std::vector<PBind>       findByDisplayKey(std::string_view displayKey) const;
        void                     clear();

        std::span<const PBind>   binds() const;
        std::span<const PSubmap> submaps() const;
        bool                     contains(const PBind& bind) const;
        bool                     empty() const;
        size_t                   size() const;
        bool                     hasSubmap(std::string_view submap) const;
        std::optional<PSubmap>   findSubmap(std::string_view submap, const WP<IHID> device) const;
        PBind                    findShortcutConflict(xkb_keysym_t keysym, Input::ModifierMask modifiers, xkb_state* xkbState = nullptr) const;

      private:
        static std::string   normalizeDisplayKey(std::string_view displayKey);

        std::vector<PBind>   m_binds;
        std::vector<PSubmap> m_submaps;
    };
}
