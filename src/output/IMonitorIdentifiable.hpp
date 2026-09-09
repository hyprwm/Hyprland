#pragma once

#include "IMonitorAddressable.hpp"
#include "../SharedDefs.hpp"

namespace Monitor {
    class IMonitorIdentifiable : public virtual IMonitorAddressable {
      public:
        virtual ~IMonitorIdentifiable() = default;

        virtual MONITORID id() const = 0;
    };
}
