#pragma once

#include "devices/IHID.hpp"
#include "helpers/memory/Memory.hpp"
#include <unordered_set>
#include <vector>
#include <string>

namespace Keybinds {

    struct SSubmapArgs {
        std::unordered_set<std::string> devices;
    };

    class CSubmap {
      public:
        CSubmap(std::string& name, SSubmapArgs metadata);

        CSubmap(CSubmap&&) noexcept                                = default;
        CSubmap& operator=(CSubmap&&) noexcept                     = default;
        CSubmap(const CSubmap&)                                    = delete;
        CSubmap&                               operator=(CSubmap&) = delete;

        bool                                   matchesDevice(SP<IHID> device);

        std::string_view                       name() const;
        const std::unordered_set<std::string>& devices() const;

      private:
        std::string                     m_name;
        std::unordered_set<std::string> m_devices;
    };

}
