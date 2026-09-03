#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>

#include "../helpers/memory/Memory.hpp"

namespace Monitor {
    class IMonitorAddressable;
}

namespace Workspace {

    using WorkspaceIDContainer = uint32_t;

    struct SWorkspaceNumberedID {
        explicit SWorkspaceNumberedID(WorkspaceIDContainer x) : value(x) {
            ;
        }

        WorkspaceIDContainer value;

        bool                 operator==(const SWorkspaceNumberedID&) const = default;
    };

    struct SWorkspaceSpecialID {
        bool operator==(const SWorkspaceSpecialID&) const = default;
    };

    using WorkspaceID = std::variant<SWorkspaceNumberedID, SWorkspaceSpecialID>;

    class IAbstractWorkspace;

    std::string_view identityTypeName(const IAbstractWorkspace& workspace);

    enum class eWorkspaceType : uint8_t {
        NORMAL,
        SPECIAL,
    };

    /*
     * An abstract workspace, this basically has very few things:
     *  - an ID
     *  - a display name
     *  - an addressable name
     *  - a monitor it belongs to
     *  - a type (can't be modified)
     */
    class IAbstractWorkspace {
      public:
        virtual ~IAbstractWorkspace() = default;

        virtual WorkspaceID                      id() const              = 0;
        virtual const std::string&               displayName() const     = 0;
        virtual const std::string&               addressableName() const = 0;
        virtual SP<Monitor::IMonitorAddressable> monitor() const         = 0;
        virtual eWorkspaceType                   type() const;

      protected:
        IAbstractWorkspace(eWorkspaceType);

      private:
        eWorkspaceType m_type = eWorkspaceType::NORMAL;
    };
}
