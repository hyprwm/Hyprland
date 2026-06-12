#pragma once

#include "../SharedDefs.hpp"

#include <string_view>

namespace Monitor {
    class IMonitorIdentifiable {
      public:
        virtual ~IMonitorIdentifiable() = default;

        virtual MONITORID        id() const                                             = 0;
        virtual std::string_view name() const                                           = 0;
        virtual std::string_view description() const                                    = 0;
        virtual std::string_view shortDescription() const                               = 0;
        virtual bool             matchesStaticSelector(std::string_view selector) const = 0;
    };
}
