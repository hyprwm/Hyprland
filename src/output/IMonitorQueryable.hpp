#pragma once

#include "IMonitorGeometry.hpp"
#include "IMonitorIdentifiable.hpp"
#include "../helpers/memory/Memory.hpp"

namespace Aquamarine {
    class IOutput;
}

namespace Monitor {
    class IMonitorQueryable : public virtual IMonitorIdentifiable, public virtual IMonitorGeometry {
      public:
        virtual bool                    enabled() const   = 0;
        virtual bool                    hasOutput() const = 0;
        virtual SP<Aquamarine::IOutput> output() const    = 0;
    };
}
