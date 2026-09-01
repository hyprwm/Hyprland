#pragma once

#include <cstdint>
#include <functional>
#include <string_view>

namespace Overview::Hyprland {
    enum class eQueryMode : uint8_t {
        ALL       = 0,
        WINDOW    = 1,
        WORKSPACE = 2,
    };

    namespace Mode {
        enum class eWorkspaceMatch : uint8_t {
            NONE,
            MATCH,
            EXACT,
        };

        using FWorkspaceSelector = std::function<bool(std::string_view)>;

        class IQueryMode {
          public:
            virtual ~IQueryMode();

            virtual eQueryMode      type() const                                                                                            = 0;
            virtual eWorkspaceMatch matchWorkspace(std::string_view name, std::string_view query, const FWorkspaceSelector& selector) const = 0;
            virtual bool            matchesWindow(std::string_view appID, std::string_view title, std::string_view query) const             = 0;
        };
    }
}
