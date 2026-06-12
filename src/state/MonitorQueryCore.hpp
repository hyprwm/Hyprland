#pragma once

#include "../helpers/math/Direction.hpp"
#include "../helpers/memory/Memory.hpp"
#include "../output/IMonitorQueryable.hpp"

#include <optional>
#include <span>
#include <string_view>

namespace Aquamarine {
    class IOutput;
}

namespace State {
    class CMonitorQueryCore {
      public:
        CMonitorQueryCore(std::span<const SP<Monitor::IMonitorQueryable>> monitors);
        ~CMonitorQueryCore() = default;

        CMonitorQueryCore(const CMonitorQueryCore&) = delete;
        CMonitorQueryCore(CMonitorQueryCore&)       = delete;
        CMonitorQueryCore(CMonitorQueryCore&&)      = delete;

        CMonitorQueryCore&&            id(MONITORID id) &&;
        CMonitorQueryCore&&            name(std::string_view name) &&;
        CMonitorQueryCore&&            description(std::string_view desc) &&;
        CMonitorQueryCore&&            selector(std::string_view str) &&;
        CMonitorQueryCore&&            configString(std::string_view str) &&;
        CMonitorQueryCore&&            vec(Vector2D vec) &&;
        CMonitorQueryCore&&            output(SP<Aquamarine::IOutput> output) &&;
        CMonitorQueryCore&&            inDirection(Math::eDirection dir) &&;
        CMonitorQueryCore&&            relativeTo(SP<Monitor::IMonitorQueryable> reference) &&;

        SP<Monitor::IMonitorQueryable> run() &&;

      private:
        SP<Monitor::IMonitorQueryable>                  closestTo(const Vector2D& vec) const;
        SP<Monitor::IMonitorQueryable>                  directionLookup(SP<Monitor::IMonitorQueryable> ref, Math::eDirection dir) const;
        SP<Monitor::IMonitorQueryable>                  fromConfigString(std::string_view sv) const;

        std::span<const SP<Monitor::IMonitorQueryable>> m_monitors;

        std::optional<MONITORID>                        m_id;
        std::optional<std::string_view>                 m_name, m_desc, m_str, m_configString;
        std::optional<SP<Aquamarine::IOutput>>          m_output;
        SP<Monitor::IMonitorQueryable>                  m_relativeTo;
        std::optional<Math::eDirection>                 m_dir;
        std::optional<Vector2D>                         m_vec;
    };
}
