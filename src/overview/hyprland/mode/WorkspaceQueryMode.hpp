#pragma once

#include "IQueryMode.hpp"

namespace Overview::Hyprland::Mode {
    class CWorkspaceQueryMode : public IQueryMode {
      public:
        virtual ~CWorkspaceQueryMode() override;

        virtual eQueryMode      type() const override;
        virtual eWorkspaceMatch matchWorkspace(std::string_view name, std::string_view query, const FWorkspaceSelector& selector) const override;
        virtual bool            matchesWindow(std::string_view appID, std::string_view title, std::string_view query) const override;
    };
}
