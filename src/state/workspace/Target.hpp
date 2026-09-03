#pragma once

#include "../../workspace/AbstractWorkspace.hpp"

#include <optional>
#include <string>

namespace State::Workspace {
    struct STarget {
        std::optional<::Workspace::WorkspaceID> id;
        std::string                             address;
        std::string                             displayName;
        ::Workspace::eWorkspaceType             type = ::Workspace::eWorkspaceType::NORMAL;

        bool                                    valid() const {
            if (!id || address.empty())
                return false;

            const auto NUMBERED = std::get_if<::Workspace::SWorkspaceNumberedID>(&*id);
            return !NUMBERED || (NUMBERED->value != 0 && type == ::Workspace::eWorkspaceType::NORMAL);
        }
    };
}
