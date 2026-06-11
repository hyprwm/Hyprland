#include "MonitorQuery.hpp"
#include "MonitorStateTracker.hpp"

#include <utility>
#include <hyprutils/string/String.hpp>
#include <hyprutils/string/Numeric.hpp>

using namespace State;
using namespace Hyprutils::String;

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
    if (m_vec && !m_desc && !m_str && !m_id && !m_name && !m_output && !m_dir) {
        // legacy path for closest-monitor
        return closestTo(*m_vec);
    }

    if (m_dir) {
        // path for getting a monitor in a direction
        return directionLookup(m_relativeTo.value_or(nullptr), *m_dir);
    }

    if (m_configString) {
        // path for getting from a config string
        return fromConfigString(*m_configString);
    }

    for (const auto& m : mons()) {
        if (m_desc && !m->m_description.starts_with(*m_desc))
            continue;

        if (m_name && *m_name != m->m_name)
            continue;

        if (m_id && *m_id != m->m_id)
            continue;

        if (m_output && *m_output != m->m_output)
            continue;

        if (m_vec && !m->logicalBox().containsPoint(*m_vec))
            continue;

        if (m_str && !m->matchesStaticSelector(*m_str))
            continue;

        return m;
    }

    return nullptr;
}

const std::vector<PHLMONITOR>& CMonitorQuery::mons() const {
    return m_includeDisabled ? m_tracker.allMonitors() : m_tracker.monitors();
}

PHLMONITOR CMonitorQuery::closestTo(const Vector2D& vec) const {
    PHLMONITOR mon;
    for (auto const& m : mons()) {
        if (CBox{m->m_position, m->m_size}.containsPoint(vec)) {
            mon = m;
            break;
        }
    }

    if (!mon) {
        float      bestDistance = 0.f;
        PHLMONITOR pBestMon;

        for (auto const& m : mons()) {
            float dist = vecToRectDistanceSquared(vec, m->m_position, m->m_position + m->m_size);

            if (dist < bestDistance || !pBestMon) {
                bestDistance = dist;
                pBestMon     = m;
            }
        }

        if (!pBestMon) { // ?????
            Log::logger->log(Log::WARN, "CMonitorQuery::closestTo: no close mon???");
            return nullptr;
        }

        return pBestMon;
    }

    return mon;
}

PHLMONITOR CMonitorQuery::directionLookup(PHLMONITOR ref, Math::eDirection dir) const {
    if (!ref)
        return nullptr;

    const auto POSA  = ref->m_position;
    const auto SIZEA = ref->m_size;

    auto       longestIntersect        = -1;
    PHLMONITOR longestIntersectMonitor = nullptr;

    for (auto const& m : mons()) {
        if (m == ref)
            continue;

        const auto POSB  = m->m_position;
        const auto SIZEB = m->m_size;
        switch (dir) {
            case Math::DIRECTION_LEFT:
                if (STICKS(POSA.x, POSB.x + SIZEB.x)) {
                    const auto INTERSECTLEN = std::max(0.0, std::min(POSA.y + SIZEA.y, POSB.y + SIZEB.y) - std::max(POSA.y, POSB.y));
                    if (INTERSECTLEN > longestIntersect) {
                        longestIntersect        = INTERSECTLEN;
                        longestIntersectMonitor = m;
                    }
                }
                break;
            case Math::DIRECTION_RIGHT:
                if (STICKS(POSA.x + SIZEA.x, POSB.x)) {
                    const auto INTERSECTLEN = std::max(0.0, std::min(POSA.y + SIZEA.y, POSB.y + SIZEB.y) - std::max(POSA.y, POSB.y));
                    if (INTERSECTLEN > longestIntersect) {
                        longestIntersect        = INTERSECTLEN;
                        longestIntersectMonitor = m;
                    }
                }
                break;
            case Math::DIRECTION_UP:
                if (STICKS(POSA.y, POSB.y + SIZEB.y)) {
                    const auto INTERSECTLEN = std::max(0.0, std::min(POSA.x + SIZEA.x, POSB.x + SIZEB.x) - std::max(POSA.x, POSB.x));
                    if (INTERSECTLEN > longestIntersect) {
                        longestIntersect        = INTERSECTLEN;
                        longestIntersectMonitor = m;
                    }
                }
                break;
            case Math::DIRECTION_DOWN:
                if (STICKS(POSA.y + SIZEA.y, POSB.y)) {
                    const auto INTERSECTLEN = std::max(0.0, std::min(POSA.x + SIZEA.x, POSB.x + SIZEB.x) - std::max(POSA.x, POSB.x));
                    if (INTERSECTLEN > longestIntersect) {
                        longestIntersect        = INTERSECTLEN;
                        longestIntersectMonitor = m;
                    }
                }
                break;
            default: break;
        }
    }

    if (longestIntersect != -1)
        return longestIntersectMonitor;

    return nullptr;
}

PHLMONITOR CMonitorQuery::fromConfigString(std::string_view sv) const {
    if (sv.empty())
        return nullptr;

    if (sv == "current")
        return m_relativeTo.value_or(nullptr);
    else if (!sv.empty() && isDirection(sv[0]))
        return directionLookup(m_relativeTo.value_or(nullptr), Math::fromChar(sv[0]));
    else if (sv[0] == '+' || sv[0] == '-') {
        // relative

        if (mons().size() == 1)
            return *mons().begin();

        const auto OFFSET = sv[0] == '-' ? sv : sv.substr(1);

        if (!isNumber(std::string{OFFSET})) {
            Log::logger->log(Log::ERR, "Error in CMonitorQuery::fromConfigString: Not a number in relative.");
            return nullptr;
        }

        int offsetLeft = strToNumber<int>(OFFSET).value_or(0);
        offsetLeft     = offsetLeft < 0 ? -((-offsetLeft) % mons().size()) : offsetLeft % mons().size();

        int currentPlace = 0;
        for (int i = 0; i < sc<int>(mons().size()); i++) {
            if (mons()[i] == m_relativeTo.value_or(nullptr)) {
                currentPlace = i;
                break;
            }
        }

        currentPlace += offsetLeft;

        if (currentPlace < 0)
            currentPlace = mons().size() + currentPlace;
        else
            currentPlace = currentPlace % mons().size();

        if (currentPlace != std::clamp(currentPlace, 0, sc<int>(mons().size()) - 1)) {
            Log::logger->log(Log::WARN, "Error in CMonitorQuery::fromConfigString: Vaxry's code sucks.");
            currentPlace = std::clamp(currentPlace, 0, sc<int>(mons().size()) - 1);
        }

        return mons()[currentPlace];
    } else if (isNumber(std::string{sv})) {
        // change by ID
        auto monID = strToNumber<MONITORID>(sv);
        if (!monID) {
            // shouldn't happen but jic
            Log::logger->log(Log::ERR, "Error in CMonitorQuery::fromConfigString: invalid num");
            return nullptr;
        }

        if (*monID > -1 && *monID < sc<MONITORID>(mons().size())) {
            return CMonitorQuery{m_tracker}.id(*monID).run();
        } else {
            Log::logger->log(Log::ERR, "Error in CMonitorQuery::fromConfigString: invalid arg 1");
            return nullptr;
        }
    } else {
        for (auto const& m : mons()) {
            if (!m->m_output)
                continue;

            if (m->matchesStaticSelector(sv))
                return m;
        }
    }

    return nullptr;
}
