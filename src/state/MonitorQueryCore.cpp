#include "MonitorQueryCore.hpp"

#include "../debug/log/Logger.hpp"
#include "../helpers/MiscFunctions.hpp"
#include "../macros.hpp"

#include <algorithm>
#include <hyprutils/string/Numeric.hpp>
#include <hyprutils/string/String.hpp>
#include <utility>

using namespace State;
using namespace Hyprutils::String;

CMonitorQueryCore::CMonitorQueryCore(std::span<const SP<Monitor::IMonitorQueryable>> monitors) : m_monitors(monitors) {
    ;
}

CMonitorQueryCore&& CMonitorQueryCore::id(MONITORID id) && {
    m_id = id;
    return std::move(*this);
}

CMonitorQueryCore&& CMonitorQueryCore::name(std::string_view name) && {
    m_name = name;
    return std::move(*this);
}

CMonitorQueryCore&& CMonitorQueryCore::description(std::string_view desc) && {
    m_desc = desc;
    return std::move(*this);
}

CMonitorQueryCore&& CMonitorQueryCore::selector(std::string_view str) && {
    m_str = str;
    return std::move(*this);
}

CMonitorQueryCore&& CMonitorQueryCore::configString(std::string_view str) && {
    m_configString = str;
    return std::move(*this);
}

CMonitorQueryCore&& CMonitorQueryCore::vec(Vector2D vec) && {
    m_vec = vec;
    return std::move(*this);
}

CMonitorQueryCore&& CMonitorQueryCore::output(SP<Aquamarine::IOutput> output) && {
    m_output = output;
    return std::move(*this);
}

CMonitorQueryCore&& CMonitorQueryCore::relativeTo(SP<Monitor::IMonitorQueryable> reference) && {
    m_relativeTo = reference;
    return std::move(*this);
}

CMonitorQueryCore&& CMonitorQueryCore::inDirection(Math::eDirection dir) && {
    m_dir = dir;
    return std::move(*this);
}

SP<Monitor::IMonitorQueryable> CMonitorQueryCore::run() && {
    if (m_vec && !m_desc && !m_str && !m_id && !m_name && !m_output && !m_dir)
        return closestTo(*m_vec);

    if (m_dir)
        return directionLookup(m_relativeTo, *m_dir);

    if (m_configString)
        return fromConfigString(*m_configString);

    for (const auto& m : m_monitors) {
        if (m_desc && !m->description().starts_with(*m_desc))
            continue;

        if (m_name && *m_name != m->name())
            continue;

        if (m_id && *m_id != m->id())
            continue;

        if (m_output && *m_output != m->output())
            continue;

        if (m_vec && !m->logicalBox().containsPoint(*m_vec))
            continue;

        if (m_str && !m->matchesStaticSelector(*m_str))
            continue;

        return m;
    }

    return nullptr;
}

SP<Monitor::IMonitorQueryable> CMonitorQueryCore::closestTo(const Vector2D& vec) const {
    SP<Monitor::IMonitorQueryable> mon;
    for (auto const& m : m_monitors) {
        if (m->logicalBox().containsPoint(vec)) {
            mon = m;
            break;
        }
    }

    if (!mon) {
        float                          bestDistance = 0.F;
        SP<Monitor::IMonitorQueryable> pBestMon;

        for (auto const& m : m_monitors) {
            const auto BOX  = m->logicalBox();
            float      dist = vecToRectDistanceSquared(vec, BOX.pos(), BOX.pos() + BOX.size());

            if (dist < bestDistance || !pBestMon) {
                bestDistance = dist;
                pBestMon     = m;
            }
        }

        if (!pBestMon) {
            Log::logger->log(Log::WARN, "CMonitorQueryCore::closestTo: no close mon???");
            return nullptr;
        }

        return pBestMon;
    }

    return mon;
}

SP<Monitor::IMonitorQueryable> CMonitorQueryCore::directionLookup(SP<Monitor::IMonitorQueryable> ref, Math::eDirection dir) const {
    if (!ref)
        return nullptr;

    const auto                     POSA  = ref->position();
    const auto                     SIZEA = ref->size();

    auto                           longestIntersect        = -1;
    SP<Monitor::IMonitorQueryable> longestIntersectMonitor = nullptr;

    for (auto const& m : m_monitors) {
        if (m == ref)
            continue;

        const auto POSB  = m->position();
        const auto SIZEB = m->size();
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

SP<Monitor::IMonitorQueryable> CMonitorQueryCore::fromConfigString(std::string_view sv) const {
    if (sv.empty())
        return nullptr;

    if (sv == "current")
        return m_relativeTo;
    else if (isDirection(sv))
        return directionLookup(m_relativeTo, Math::fromChar(sv[0]));
    else if (sv[0] == '+' || sv[0] == '-') {
        if (m_monitors.size() == 1)
            return *m_monitors.begin();

        const auto OFFSET = sv[0] == '-' ? sv : sv.substr(1);

        if (!isNumber(std::string{OFFSET})) {
            Log::logger->log(Log::ERR, "Error in CMonitorQueryCore::fromConfigString: Not a number in relative.");
            return nullptr;
        }

        int offsetLeft = strToNumber<int>(OFFSET).value_or(0);
        offsetLeft     = offsetLeft < 0 ? -((-offsetLeft) % m_monitors.size()) : offsetLeft % m_monitors.size();

        int currentPlace = 0;
        for (int i = 0; i < sc<int>(m_monitors.size()); i++) {
            if (m_monitors[i] == m_relativeTo) {
                currentPlace = i;
                break;
            }
        }

        currentPlace += offsetLeft;

        if (currentPlace < 0)
            currentPlace = m_monitors.size() + currentPlace;
        else
            currentPlace = currentPlace % m_monitors.size();

        if (currentPlace != std::clamp(currentPlace, 0, sc<int>(m_monitors.size()) - 1)) {
            Log::logger->log(Log::WARN, "Error in CMonitorQueryCore::fromConfigString: Vaxry's code sucks.");
            currentPlace = std::clamp(currentPlace, 0, sc<int>(m_monitors.size()) - 1);
        }

        return m_monitors[currentPlace];
    } else if (isNumber(std::string{sv})) {
        auto monID = strToNumber<MONITORID>(sv);
        if (!monID) {
            Log::logger->log(Log::ERR, "Error in CMonitorQueryCore::fromConfigString: invalid num");
            return nullptr;
        }

        if (*monID > -1 && *monID < sc<MONITORID>(m_monitors.size()))
            return CMonitorQueryCore{m_monitors}.id(*monID).run();

        Log::logger->log(Log::ERR, "Error in CMonitorQueryCore::fromConfigString: invalid arg 1");
        return nullptr;
    } else {
        for (auto const& m : m_monitors) {
            if (!m->hasOutput())
                continue;

            if (m->matchesStaticSelector(sv))
                return m;
        }
    }

    return nullptr;
}
