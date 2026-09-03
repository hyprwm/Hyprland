#include "Resolver.hpp"
#include "State.hpp"

#include "../../workspace/HLWorkspace.hpp"
#include "../../desktop/state/FocusState.hpp"
#include "../../desktop/history/WorkspaceHistoryTracker.hpp"
#include "../../config/shared/workspace/WorkspaceRuleManager.hpp"
#include "../MonitorState.hpp"

#include <hyprutils/string/Numeric.hpp>
#include <hyprutils/string/String.hpp>

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <optional>
#include <set>
#include <vector>

using namespace Hyprutils::String;
using namespace State::Workspace;
using namespace State;

UP<CWorkspaceResolver>& State::Workspace::resolver() {
    static UP<CWorkspaceResolver> resolver = makeUnique<CWorkspaceResolver>();
    return resolver;
}

static State::Workspace::STarget targetFor(const PHLWORKSPACE& workspace) {
    if (!workspace)
        return {};

    return {
        .id          = workspace->id(),
        .address     = workspace->addressableName(),
        .displayName = workspace->displayName(),
        .type        = workspace->type(),
    };
}

static State::Workspace::STarget numberedTarget(int64_t id) {
    if (id <= 0 || id > UINT32_MAX)
        return {};

    const auto ADDRESS = std::to_string(id);
    return {
        .id          = ::Workspace::SWorkspaceNumberedID{sc<::Workspace::WorkspaceIDContainer>(id)},
        .address     = ADDRESS,
        .displayName = ADDRESS,
    };
}

static std::optional<int64_t> workspaceOffset(std::string_view value) {
    if (value.starts_with('+'))
        value.remove_prefix(1);
    if (value.empty())
        return std::nullopt;

    int64_t result          = 0;
    const auto [END, ERROR] = std::from_chars(value.data(), value.data() + value.size(), result);
    if (ERROR != std::errc{} || END != value.data() + value.size() || result < -sc<int64_t>(UINT32_MAX) || result > UINT32_MAX)
        return std::nullopt;

    return result;
}

static std::optional<int64_t> availableWorkspaceAt(uint64_t position, const std::set<int64_t>& unavailable) {
    uint64_t result = position;
    for (const auto BLOCKED : unavailable) {
        if (BLOCKED <= 0)
            continue;
        if (sc<uint64_t>(BLOCKED) > result)
            break;
        if (++result > UINT32_MAX)
            return std::nullopt;
    }

    return sc<int64_t>(result);
}

static std::optional<int64_t> offsetAvailableWorkspace(uint32_t current, int64_t offset, const std::set<int64_t>& unavailable) {
    if (offset >= 0) {
        uint64_t result = sc<uint64_t>(current) + sc<uint64_t>(offset);
        if (result > UINT32_MAX)
            return std::nullopt;

        for (const auto BLOCKED : unavailable) {
            if (BLOCKED <= current)
                continue;
            if (sc<uint64_t>(BLOCKED) > result)
                break;
            if (++result > UINT32_MAX)
                return std::nullopt;
        }

        return sc<int64_t>(result);
    }

    const auto DISTANCE         = sc<uint64_t>(-offset);
    const auto BLOCKED_BEFORE   = std::ranges::count_if(unavailable, [current](const auto blocked) { return blocked > 0 && blocked < current; });
    const auto AVAILABLE_BEFORE = sc<uint64_t>(current - 1) - sc<uint64_t>(BLOCKED_BEFORE);
    if (DISTANCE > AVAILABLE_BEFORE)
        return std::nullopt;

    int64_t result = sc<int64_t>(current) - sc<int64_t>(DISTANCE);
    for (auto it = unavailable.rbegin(); it != unavailable.rend(); ++it) {
        if (*it >= current)
            continue;
        if (*it < result)
            break;
        --result;
    }

    return result;
}

State::Workspace::STarget CWorkspaceResolver::getWorkspaceTargetFromString(const std::string& in, std::optional<PHLMONITOR> baseMon) {
    const auto BASEMONITOR = baseMon.value_or(Desktop::focusState()->monitor());
    if (in.empty())
        return {};

    if (in == "special")
        return {.id = ::Workspace::SWorkspaceSpecialID{}, .address = "special:special", .displayName = "special:special", .type = ::Workspace::eWorkspaceType::SPECIAL};

    if (in.starts_with("special:")) {
        if (in.size() == 8)
            return {};
        return {.id = ::Workspace::SWorkspaceSpecialID{}, .address = in, .displayName = in, .type = ::Workspace::eWorkspaceType::SPECIAL};
    }

    if (in.starts_with("name:")) {
        const auto ADDRESS = in.substr(5);
        if (ADDRESS.empty())
            return {};
        return {.id = ::Workspace::SWorkspaceSpecialID{}, .address = ADDRESS, .displayName = ADDRESS, .type = ::Workspace::eWorkspaceType::NORMAL};
    }

    if (in.starts_with("empty")) {
        const bool SAME_MONITOR = in.substr(5).contains("m");
        const bool NEXT         = in.substr(5).contains("n");
        if ((SAME_MONITOR || NEXT) && !BASEMONITOR) {
            LOG(Log::ERR, "Empty monitor workspace on monitor null!");
            return {};
        }

        std::set<int64_t> unavailable;
        if (SAME_MONITOR) {
            for (const auto& rule : Config::workspaceRuleMgr()->getAllWorkspaceRules()) {
                const auto ID = strToNumber<::Workspace::WorkspaceIDContainer>(rule->m_workspaceString);
                if (!rule->isEnabled() || !ID || *ID == 0)
                    continue;

                const auto MONITOR = State::monitorState()->query().relativeTo(BASEMONITOR).configString(rule->m_monitor).run();
                if (MONITOR && MONITOR != BASEMONITOR)
                    unavailable.insert(*ID);
            }
        }

        int64_t id = NEXT && BASEMONITOR->m_activeWorkspace && BASEMONITOR->m_activeWorkspace->numberedID() ? *BASEMONITOR->m_activeWorkspace->numberedID() : 0;
        while (++id <= UINT32_MAX) {
            const auto WORKSPACE = State::Workspace::state()->query().numbered(::Workspace::SWorkspaceNumberedID{sc<uint32_t>(id)}).run();
            if (!unavailable.contains(id) && (!WORKSPACE || WORKSPACE->getWindowCount() == 0))
                return numberedTarget(id);
        }
        return {};
    }

    if (in.starts_with("prev")) {
        if (!BASEMONITOR || !valid(BASEMONITOR->m_activeWorkspace))
            return {};
        return Desktop::History::workspaceTracker()->previousWorkspace(BASEMONITOR->m_activeWorkspace).target;
    }

    if (in == "next") {
        if (!BASEMONITOR || !BASEMONITOR->m_activeWorkspace) {
            LOG(Log::ERR, "no active monitor or workspace for 'next'");
            return {};
        }

        const auto CURRENT = BASEMONITOR->m_activeWorkspace->numberedID();
        return CURRENT && *CURRENT < UINT32_MAX ? numberedTarget(*CURRENT + 1) : State::Workspace::STarget{};
    }

    if (in.size() > 2 && in[0] == 'r' && (in[1] == '-' || in[1] == '+' || in[1] == '~') && isNumber(in.substr(2))) {
        const bool ABSOLUTE = in[1] == '~';
        if (!BASEMONITOR)
            return {};

        const auto OFFSET = workspaceOffset(in.substr(ABSOLUTE ? 2 : 1));
        if (!OFFSET)
            return {};

        int64_t                   remains = *OFFSET;
        std::set<int64_t>         unavailable;
        std::vector<PHLWORKSPACE> named;

        for (const auto& weak : State::Workspace::state()->workspaces()) {
            const auto workspace = weak.lock();
            const auto numbered  = workspace->numberedID();
            if (numbered && (workspace->type() == ::Workspace::eWorkspaceType::SPECIAL || workspace->m_monitor != BASEMONITOR))
                unavailable.insert(*numbered);
            else if (!numbered && workspace->type() != ::Workspace::eWorkspaceType::SPECIAL && workspace->m_monitor == BASEMONITOR)
                named.emplace_back(workspace);
        }
        std::ranges::reverse(named);

        for (const auto& rule : Config::workspaceRuleMgr()->getAllWorkspaceRules()) {
            const auto ID = strToNumber<::Workspace::WorkspaceIDContainer>(rule->m_workspaceString);
            if (!rule->isEnabled() || !ID || *ID == 0)
                continue;
            const auto MONITOR = State::monitorState()->query().relativeTo(BASEMONITOR).configString(rule->m_monitor).run();
            if (MONITOR && MONITOR != BASEMONITOR)
                unavailable.insert(*ID);
        }

        if (ABSOLUTE) {
            remains--;
            if (remains >= 0 && sc<size_t>(remains) < named.size())
                return targetFor(named[remains]);

            remains -= named.size();
            if (remains < 0)
                return {};
            const auto ID = availableWorkspaceAt(sc<uint64_t>(remains) + 1, unavailable);
            return ID ? numberedTarget(*ID) : State::Workspace::STarget{};
        }

        const auto ACTIVE_NUMBERED = BASEMONITOR->m_activeWorkspace ? BASEMONITOR->m_activeWorkspace->numberedID() : std::optional<::Workspace::WorkspaceIDContainer>{1};
        if (!ACTIVE_NUMBERED) {
            const auto CURRENT = std::ranges::find(named, BASEMONITOR->m_activeWorkspace);
            auto       index   = CURRENT == named.end() ? sc<int64_t>(named.size()) : sc<int64_t>(std::ranges::distance(named.begin(), CURRENT));
            index += remains;
            if (index < 0)
                index = 0;
            if (index < sc<int64_t>(named.size()))
                return targetFor(named[index]);

            remains = index - sc<int64_t>(named.size()) + 1;
        }

        const auto ID = offsetAvailableWorkspace(ACTIVE_NUMBERED.value_or(0), remains, unavailable);
        if (ID)
            return numberedTarget(*ID);
        if (remains < 0 && !named.empty())
            return targetFor(named.back());
        return remains < 0 ? numberedTarget(1) : State::Workspace::STarget{};
    }

    if (in.size() > 2 && (in[0] == 'm' || in[0] == 'e') && (in[1] == '-' || in[1] == '+' || in[1] == '~') && isNumber(in.substr(2))) {
        const bool ALL_MONITORS = in[0] == 'e';
        const bool ABSOLUTE     = in[1] == '~';
        if (!BASEMONITOR)
            return {};

        const auto OFFSET = workspaceOffset(in.substr(ABSOLUTE ? 2 : 1));
        if (!OFFSET)
            return {};

        std::vector<PHLWORKSPACE> named, numbered;
        for (const auto& weak : State::Workspace::state()->workspaces()) {
            const auto workspace = weak.lock();
            if (workspace->type() == ::Workspace::eWorkspaceType::SPECIAL || (workspace->m_monitor != BASEMONITOR && !ALL_MONITORS))
                continue;
            (workspace->numberedID() ? numbered : named).emplace_back(workspace);
        }
        std::ranges::reverse(named);
        std::ranges::sort(numbered, {}, [](const auto& workspace) { return *workspace->numberedID(); });
        named.insert(named.end(), numbered.begin(), numbered.end());
        if (named.empty())
            return {};

        auto index = sc<int64_t>(*OFFSET) - (ABSOLUTE ? 1 : 0);
        if (!ABSOLUTE) {
            const auto CURRENT = std::ranges::find(named, BASEMONITOR->m_activeWorkspace);
            index += CURRENT == named.end() ? 0 : std::ranges::distance(named.begin(), CURRENT);
            index %= sc<int64_t>(named.size());
            if (index < 0)
                index += sc<int64_t>(named.size());
        } else
            index = std::clamp(index, sc<int64_t>(0), sc<int64_t>(named.size() - 1));

        return targetFor(named[index]);
    }

    if (in[0] == '+' || in[0] == '-') {
        if (!BASEMONITOR || !BASEMONITOR->m_activeWorkspace || !BASEMONITOR->m_activeWorkspace->numberedID()) {
            LOG(Log::ERR, "Relative workspace on no numbered workspace!");
            return {};
        }

        const auto OFFSET = workspaceOffset(in);
        if (!OFFSET)
            return {};
        const auto VALUE = sc<int64_t>(*BASEMONITOR->m_activeWorkspace->numberedID()) + *OFFSET;
        return numberedTarget(std::max(VALUE, sc<int64_t>(1)));
    }

    if (isNumber(in)) {
        const auto ID = Hyprutils::String::strToNumber<int64_t>(in);
        return ID ? numberedTarget(std::max(*ID, sc<int64_t>(1))) : State::Workspace::STarget{};
    }

    return targetFor(State::Workspace::state()->query().input(in).run());
}
