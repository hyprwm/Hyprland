#pragma once

#include "devices/IHID.hpp"
#include "helpers/memory/Memory.hpp"
#include <unordered_set>
#include <vector>
#include <string>

namespace Keybinds {

    struct SSubmapArgs {
        std::unordered_set<std::string> devices;
        bool                            inclusive;
    };

    class CSubmap {
      public:
        CSubmap(std::string& name, SSubmapArgs args);

        CSubmap(CSubmap&&) noexcept                                = default;
        CSubmap& operator=(CSubmap&&) noexcept                     = default;
        CSubmap(const CSubmap&)                                    = delete;
        CSubmap&                               operator=(CSubmap&) = delete;

        bool                                   matchesDevice(SP<IHID> device) const;

        std::string_view                       name() const;
        bool                                   inclusive() const;
        const std::unordered_set<std::string>& devices() const;

      private:
        std::string                     m_name;
        std::unordered_set<std::string> m_devices;
        bool                            m_inclusive;
    };

}
