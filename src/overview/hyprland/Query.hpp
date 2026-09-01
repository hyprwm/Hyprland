#pragma once

#include "../../helpers/memory/Memory.hpp"
#include "mode/IQueryMode.hpp"

#include <array>
#include <string>
#include <string_view>

namespace Overview::Hyprland {
    struct SQueryConfig {
        char       windowPrefix    = '/';
        char       workspacePrefix = '.';
        eQueryMode defaultMode     = eQueryMode::ALL;
    };

    class CQuery {
      public:
        CQuery(std::string raw, const SQueryConfig& config = {});
        ~CQuery();

        Mode::eWorkspaceMatch matchWorkspace(std::string_view name, const Mode::FWorkspaceSelector& selector = {}) const;
        bool                  matchesWindow(std::string_view appID, std::string_view title) const;
        bool                  usesWindowMetadata() const;
        bool                  empty() const;
        eQueryMode            mode() const;
        eQueryMode            mode(std::string_view query) const;
        const std::string&    raw() const;
        const std::string&    term() const;

      private:
        std::string                         m_raw;
        std::string                         m_term;
        SQueryConfig                        m_config;
        std::array<UP<Mode::IQueryMode>, 3> m_modes;
    };
}
