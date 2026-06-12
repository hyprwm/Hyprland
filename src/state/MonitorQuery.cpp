#include "MonitorQuery.hpp"
#include "MonitorQueryCore.hpp"
#include "MonitorStateTracker.hpp"

#include <utility>
#include <vector>

using namespace State;

CMonitorQuery::CMonitorQuery(const IMonitorStateTracker& t) : m_tracker(t) {
    ;
}

CMonitorQuery&& CMonitorQuery::id(MONITORID id) && {
    m_id = id;
    return std::move(*this);
}

CMonitorQuery&& CMonitorQuery::name(std::string_view name) && {
    m_name = name;
    return std::move(*this);
}

CMonitorQuery&& CMonitorQuery::description(std::string_view desc) && {
    m_desc = desc;
    return std::move(*this);
}

CMonitorQuery&& CMonitorQuery::selector(std::string_view str) && {
    m_str = str;
    return std::move(*this);
}

CMonitorQuery&& CMonitorQuery::configString(std::string_view str) && {
    m_configString = str;
    return std::move(*this);
}

CMonitorQuery&& CMonitorQuery::vec(Vector2D vec) && {
    m_vec = vec;
    return std::move(*this);
}

CMonitorQuery&& CMonitorQuery::output(SP<Aquamarine::IOutput> output) && {
    m_output = output;
    return std::move(*this);
}

CMonitorQuery&& CMonitorQuery::relativeTo(PHLMONITOR reference) && {
    m_relativeTo = reference;
    return std::move(*this);
}

CMonitorQuery&& CMonitorQuery::inDirection(Math::eDirection dir) && {
    m_dir = dir;
    return std::move(*this);
}

CMonitorQuery&& CMonitorQuery::includeDisabled(bool inc) && {
    m_includeDisabled = inc;
    return std::move(*this);
}

PHLMONITOR CMonitorQuery::run() && {
    std::vector<SP<Monitor::IMonitorQueryable>> queryableMonitors;
    queryableMonitors.reserve(mons().size());
    for (const auto& m : mons()) {
        auto queryable = dynamicPointerCast<Monitor::IMonitorQueryable>(m);
        RASSERT(queryable, "CMonitor does not implement IMonitorQueryable");
        queryableMonitors.push_back(queryable);
    }

    CMonitorQueryCore core{queryableMonitors};

    if (m_id)
        std::move(core).id(*m_id);
    if (m_name)
        std::move(core).name(*m_name);
    if (m_desc)
        std::move(core).description(*m_desc);
    if (m_str)
        std::move(core).selector(*m_str);
    if (m_configString)
        std::move(core).configString(*m_configString);
    if (m_vec)
        std::move(core).vec(*m_vec);
    if (m_output)
        std::move(core).output(*m_output);
    if (m_relativeTo.has_value()) {
        SP<Monitor::IMonitorQueryable> relativeTo;
        if (*m_relativeTo) {
            relativeTo = dynamicPointerCast<Monitor::IMonitorQueryable>(*m_relativeTo);
            RASSERT(relativeTo, "CMonitor does not implement IMonitorQueryable");
        }
        std::move(core).relativeTo(relativeTo);
    }
    if (m_dir)
        std::move(core).inDirection(*m_dir);

    const auto RESULT = std::move(core).run();
    if (!RESULT)
        return nullptr;

    auto monitor = dynamicPointerCast<Monitor::CMonitor>(RESULT);
    RASSERT(monitor, "CMonitorQueryCore returned a non-CMonitor queryable in CMonitorQuery");
    return monitor;
}

const std::vector<PHLMONITOR>& CMonitorQuery::mons() const {
    return m_includeDisabled ? m_tracker.allMonitors() : m_tracker.monitors();
}
