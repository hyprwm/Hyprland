#include "Config.hpp"

#include "../Compositor.hpp"
#include "../macros.hpp"
#include "../desktop/state/FocusState.hpp"
#include "../desktop/history/WorkspaceHistoryTracker.hpp"
#include "../config/shared/workspace/WorkspaceRuleManager.hpp"
#include "../output/Monitor.hpp"

#include <hyprutils/string/String.hpp>
#include <hyprutils/string/VarList.hpp>

#include <climits>

using namespace Hyprutils::String;

static bool isAutoIDdWorkspace(WORKSPACEID id) {
    return id < WORKSPACE_INVALID;
}

bool isDirection(const std::string& arg) {
    return arg == "l" || arg == "r" || arg == "u" || arg == "d" || arg == "t" || arg == "b";
}

bool isDirection(const char& arg) {
    return arg == 'l' || arg == 'r' || arg == 'u' || arg == 'd' || arg == 't' || arg == 'b';
}

std::optional<float> getPlusMinusKeywordResult(std::string source, float relative) {
    try {
        return relative + stof(source);
    } catch (...) {
        Log::logger->log(Log::ERR, "Invalid arg \"{}\" in getPlusMinusKeywordResult!", source);
        return {};
    }
}

SWorkspaceIDName getWorkspaceIDNameFromString(const std::string& in) {
    SWorkspaceIDName result = {WORKSPACE_INVALID, ""};

    if (in.starts_with("special")) {
        result.name = "special:special";

        if (in.length() > 8) {
            const auto NAME = in.substr(8);
            const auto WS   = g_pCompositor->getWorkspaceByName("special:" + NAME);

            return {WS ? WS->m_id : g_pCompositor->getNewSpecialID(), "special:" + NAME};
        }

        result.id = SPECIAL_WORKSPACE_START;
        return result;
    } else if (in.starts_with("name:")) {
        const auto WORKSPACENAME = in.substr(in.find_first_of(':') + 1);
        const auto WORKSPACE     = g_pCompositor->getWorkspaceByName(WORKSPACENAME);
        if (!WORKSPACE) {
            result.id = g_pCompositor->getNextAvailableNamedWorkspace();
        } else {
            result.id = WORKSPACE->m_id;
        }
        result.name = WORKSPACENAME;
    } else if (in.starts_with("empty")) {
        const bool same_mon = in.substr(5).contains("m");
        const bool next     = in.substr(5).contains("n");
        if ((same_mon || next) && !Desktop::focusState()->monitor()) {
            Log::logger->log(Log::ERR, "Empty monitor workspace on monitor null!");
            return {WORKSPACE_INVALID};
        }

        std::set<WORKSPACEID> invalidWSes;
        if (same_mon) {
            for (auto const& rule : Config::workspaceRuleMgr()->getAllWorkspaceRules()) {
                const auto PMONITOR = g_pCompositor->getMonitorFromString(rule.m_monitor);
                if (PMONITOR && (PMONITOR->m_id != Desktop::focusState()->monitor()->m_id))
                    invalidWSes.insert(rule.m_workspaceId);
            }
        }

        WORKSPACEID id = next ? Desktop::focusState()->monitor()->activeWorkspaceID() : 0;
        while (++id < LONG_MAX) {
            const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(id);
            if (!invalidWSes.contains(id) && (!PWORKSPACE || PWORKSPACE->getWindows() == 0)) {
                result.id = id;
                return result;
            }
        }
    } else if (in.starts_with("previous")) {
        const auto PMONITOR = Desktop::focusState()->monitor();
        if (!PMONITOR)
            return {WORKSPACE_INVALID};

        const auto PWORKSPACE = PMONITOR->m_activeWorkspace;
        if (!valid(PWORKSPACE))
            return {WORKSPACE_INVALID};

        const auto PREVWSIDNAME = in.contains("_per_monitor") ? Desktop::History::workspaceTracker()->previousWorkspaceIDName(PWORKSPACE, PMONITOR) :
                                                                Desktop::History::workspaceTracker()->previousWorkspaceIDName(PWORKSPACE);

        if (PREVWSIDNAME.id == WORKSPACE_INVALID)
            return {WORKSPACE_INVALID};

        const auto PLASTWORKSPACE = g_pCompositor->getWorkspaceByID(PREVWSIDNAME.id);

        if (!PLASTWORKSPACE) {
            Log::logger->log(Log::DEBUG, "previous workspace {} doesn't exist yet", PREVWSIDNAME.id);
            return {PREVWSIDNAME.id, PREVWSIDNAME.name};
        }

        return {PLASTWORKSPACE->m_id, PLASTWORKSPACE->m_name};
    } else {
        if (in[0] == 'r' && (in[1] == '-' || in[1] == '+' || in[1] == '~') && isNumber(in.substr(2))) {
            bool absolute = in[1] == '~';
            if (!Desktop::focusState()->monitor()) {
                Log::logger->log(Log::ERR, "Relative monitor workspace on monitor null!");
                return {WORKSPACE_INVALID};
            }

            const auto PLUSMINUSRESULT = getPlusMinusKeywordResult(in.substr(absolute ? 2 : 1), 0);

            if (!PLUSMINUSRESULT.has_value())
                return {WORKSPACE_INVALID};

            result.id = sc<int>(PLUSMINUSRESULT.value());

            WORKSPACEID           remains = result.id;

            std::set<WORKSPACEID> invalidWSes;

            // Collect all the workspaces we can't jump to.
            for (auto const& ws : g_pCompositor->getWorkspaces()) {
                if (ws->m_isSpecialWorkspace || (ws->m_monitor != Desktop::focusState()->monitor())) {
                    // Can't jump to this workspace
                    invalidWSes.insert(ws->m_id);
                }
            }
            for (auto const& rule : Config::workspaceRuleMgr()->getAllWorkspaceRules()) {
                const auto PMONITOR = g_pCompositor->getMonitorFromString(rule.m_monitor);
                if (!PMONITOR || PMONITOR->m_id == Desktop::focusState()->monitor()->m_id) {
                    // Can't be invalid
                    continue;
                }
                // WS is bound to another monitor, can't jump to this
                invalidWSes.insert(rule.m_workspaceId);
            }

            // Prepare all named workspaces in case when we need them
            std::vector<WORKSPACEID> namedWSes;
            for (auto const& ws : g_pCompositor->getWorkspaces()) {
                if (ws->m_isSpecialWorkspace || (ws->m_monitor != Desktop::focusState()->monitor()) || ws->m_id >= 0)
                    continue;

                namedWSes.push_back(ws->m_id);
            }
            std::ranges::sort(namedWSes);

            if (absolute) {
                // 1-index
                remains -= 1;

                // traverse valid workspaces until we reach the remains
                if (sc<size_t>(remains) < namedWSes.size()) {
                    result.id = namedWSes[remains];
                } else {
                    remains -= namedWSes.size();
                    result.id = 0;
                    while (remains >= 0) {
                        result.id++;
                        if (!invalidWSes.contains(result.id)) {
                            remains--;
                        }
                    }
                }
            } else {

                // Just take a blind guess at where we'll probably end up
                WORKSPACEID activeWSID    = Desktop::focusState()->monitor()->m_activeWorkspace ? Desktop::focusState()->monitor()->m_activeWorkspace->m_id : 1;
                WORKSPACEID predictedWSID = activeWSID + remains;
                int         remainingWSes = 0;
                char        walkDir       = in[1];

                // sanitize. 0 means invalid oob in -
                predictedWSID = std::max(predictedWSID, sc<int64_t>(0));

                // Count how many invalidWSes are in between (how bad the prediction was)
                WORKSPACEID beginID = in[1] == '+' ? activeWSID + 1 : predictedWSID;
                WORKSPACEID endID   = in[1] == '+' ? predictedWSID : activeWSID;
                auto        begin   = invalidWSes.upper_bound(beginID - 1); // upper_bound is >, we want >=
                for (auto it = begin; it != invalidWSes.end() && *it <= endID; it++) {
                    remainingWSes++;
                }

                // Handle named workspaces. They are treated like always before other workspaces
                if (activeWSID < 0) {
                    // Behaviour similar to 'm'
                    // Find current
                    size_t currentItem = -1;
                    for (size_t i = 0; i < namedWSes.size(); i++) {
                        if (namedWSes[i] == activeWSID) {
                            currentItem = i;
                            break;
                        }
                    }

                    currentItem += remains;
                    currentItem = std::max(currentItem, sc<size_t>(0));
                    if (currentItem >= namedWSes.size()) {
                        // At the seam between namedWSes and normal WSes. Behave like r+[diff] at imaginary ws 0
                        size_t diff         = currentItem - (namedWSes.size() - 1);
                        predictedWSID       = diff;
                        WORKSPACEID beginID = 1;
                        WORKSPACEID endID   = predictedWSID;
                        auto        begin   = invalidWSes.upper_bound(beginID - 1); // upper_bound is >, we want >=
                        for (auto it = begin; it != invalidWSes.end() && *it <= endID; it++) {
                            remainingWSes++;
                        }
                        walkDir = '+';
                    } else {
                        // We found our final ws.
                        remainingWSes = 0;
                        predictedWSID = namedWSes[currentItem];
                    }
                }

                // Go in the search direction for remainingWSes
                // The performance impact is directly proportional to the number of open and bound workspaces
                WORKSPACEID finalWSID = predictedWSID;
                if (walkDir == '-') {
                    WORKSPACEID beginID = finalWSID;
                    WORKSPACEID curID   = finalWSID;
                    while (--curID > 0 && remainingWSes > 0) {
                        if (!invalidWSes.contains(curID)) {
                            remainingWSes--;
                        }
                        finalWSID = curID;
                    }
                    if (finalWSID <= 0 || invalidWSes.contains(finalWSID)) {
                        if (!namedWSes.empty()) {
                            // Go to the named workspaces
                            // Need remainingWSes more
                            auto namedWSIdx = namedWSes.size() - remainingWSes;
                            // Sanitze
                            namedWSIdx = std::clamp(namedWSIdx, sc<size_t>(0), namedWSes.size() - sc<size_t>(1));
                            finalWSID  = namedWSes[namedWSIdx];
                        } else {
                            // Couldn't find valid workspace in negative direction, search last first one back up positive direction
                            walkDir = '+';
                            // We know, that everything less than beginID is invalid, so don't bother with that
                            finalWSID     = beginID;
                            remainingWSes = 1;
                        }
                    }
                }
                if (walkDir == '+') {
                    WORKSPACEID curID = finalWSID;
                    while (++curID < INT32_MAX && remainingWSes > 0) {
                        if (!invalidWSes.contains(curID)) {
                            remainingWSes--;
                        }
                        finalWSID = curID;
                    }
                }
                result.id = finalWSID;
            }

            const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(result.id);
            if (PWORKSPACE)
                result.name = g_pCompositor->getWorkspaceByID(result.id)->m_name;
            else
                result.name = std::to_string(result.id);

        } else if ((in[0] == 'm' || in[0] == 'e') && (in[1] == '-' || in[1] == '+' || in[1] == '~') && isNumber(in.substr(2))) {
            bool onAllMonitors = in[0] == 'e';
            bool absolute      = in[1] == '~';

            if (!Desktop::focusState()->monitor()) {
                Log::logger->log(Log::ERR, "Relative monitor workspace on monitor null!");
                return {WORKSPACE_INVALID};
            }

            // monitor relative
            const auto PLUSMINUSRESULT = getPlusMinusKeywordResult(in.substr(absolute ? 2 : 1), 0);

            if (!PLUSMINUSRESULT.has_value())
                return {WORKSPACE_INVALID};

            result.id = sc<int>(PLUSMINUSRESULT.value());

            // result now has +/- what we should move on mon
            int                      remains = sc<int>(result.id);

            std::vector<WORKSPACEID> validWSes;
            for (auto const& ws : g_pCompositor->getWorkspaces()) {
                if (ws->m_isSpecialWorkspace || (ws->m_monitor != Desktop::focusState()->monitor() && !onAllMonitors))
                    continue;

                validWSes.push_back(ws->m_id);
            }

            std::ranges::sort(validWSes);

            ssize_t currentItem = -1;

            if (absolute) {
                // 1-index
                currentItem = remains - 1;

                // clamp
                if (currentItem < 0) {
                    currentItem = 0;
                } else if (currentItem >= sc<ssize_t>(validWSes.size())) {
                    currentItem = validWSes.size() - 1;
                }
            } else {
                // get the offset
                remains = remains < 0 ? -((-remains) % validWSes.size()) : remains % validWSes.size();

                // get the current item
                WORKSPACEID activeWSID = Desktop::focusState()->monitor()->m_activeWorkspace ? Desktop::focusState()->monitor()->m_activeWorkspace->m_id : 1;
                for (ssize_t i = 0; i < sc<ssize_t>(validWSes.size()); i++) {
                    if (validWSes[i] == activeWSID) {
                        currentItem = i;
                        break;
                    }
                }

                // apply
                currentItem += remains;

                // sanitize
                if (currentItem >= sc<ssize_t>(validWSes.size())) {
                    currentItem = currentItem % validWSes.size();
                } else if (currentItem < 0) {
                    currentItem = validWSes.size() + currentItem;
                }
            }

            result.id   = validWSes[currentItem];
            result.name = g_pCompositor->getWorkspaceByID(validWSes[currentItem])->m_name;
        } else {
            if (in[0] == '+' || in[0] == '-') {
                if (Desktop::focusState()->monitor()) {
                    const auto PLUSMINUSRESULT = getPlusMinusKeywordResult(in, Desktop::focusState()->monitor()->activeWorkspaceID());
                    if (!PLUSMINUSRESULT.has_value())
                        return {WORKSPACE_INVALID};

                    result.id = std::max(sc<int>(PLUSMINUSRESULT.value()), 1);
                } else {
                    Log::logger->log(Log::ERR, "Relative workspace on no mon!");
                    return {WORKSPACE_INVALID};
                }
            } else if (isNumber(in))
                result.id = std::max(std::stoi(in), 1);
            else {
                // maybe name
                const auto PWORKSPACE = g_pCompositor->getWorkspaceByName(in);
                if (PWORKSPACE)
                    result.id = PWORKSPACE->m_id;
            }

            result.name = std::to_string(result.id);
        }
    }

    result.isAutoIDd = isAutoIDdWorkspace(result.id);

    return result;
}

// helper: resolve workspace from string and optionally create it
PHLWORKSPACE resolveWorkspace(const std::string& args) {
    const auto& [id, name, isAutoID] = getWorkspaceIDNameFromString(args);
    if (id == WORKSPACE_INVALID)
        return nullptr;

    auto ws = g_pCompositor->getWorkspaceByID(id);
    if (!ws) {
        const auto PMONITOR = Desktop::focusState()->monitor();
        if (PMONITOR)
            ws = g_pCompositor->createNewWorkspace(id, PMONITOR->m_id, name, false);
    }

    return ws;
}

// helper: resolve workspace from string and optionally create it, taking binds:workspace_back_and_forth into account
PHLWORKSPACE resolveWorkspaceForChange(const std::string& args) {
    static auto PBACKANDFORTH = CConfigValue<Config::INTEGER>("binds:workspace_back_and_forth");

    const auto  PMONITOR = Desktop::focusState()->monitor();
    if (!PMONITOR)
        return nullptr;

    auto ws = resolveWorkspace(args);
    if (!ws)
        return nullptr;

    const auto PCURRENTWS = PMONITOR->m_activeWorkspace;
    if (!PCURRENTWS)
        return ws;

    // workspace_back_and_forth: if switching to current workspace, go to previous
    if (ws->m_id == PCURRENTWS->m_id && *PBACKANDFORTH) {
        const auto PREVWSIDNAME = Desktop::History::workspaceTracker()->previousWorkspaceIDName(PCURRENTWS);
        if (PREVWSIDNAME.id == WORKSPACE_INVALID)
            return ws;

        auto pprevws = g_pCompositor->getWorkspaceByID(PREVWSIDNAME.id);
        if (!pprevws)
            pprevws = g_pCompositor->createNewWorkspace(PREVWSIDNAME.id, PMONITOR->m_id, PREVWSIDNAME.name.empty() ? std::to_string(PREVWSIDNAME.id) : PREVWSIDNAME.name);
        return pprevws;
    }

    return ws;
}

std::optional<std::string> cleanCmdForWorkspace(const std::string& inWorkspaceName, std::string dirtyCmd) {

    std::string cmd = trim(dirtyCmd);

    if (!cmd.empty()) {
        std::string       rules;
        const std::string workspaceRule = "workspace " + inWorkspaceName;

        if (cmd[0] == '[') {
            const auto closingBracketIdx = cmd.find_last_of(']');
            auto       tmpRules          = cmd.substr(1, closingBracketIdx - 1);
            cmd                          = cmd.substr(closingBracketIdx + 1);

            auto rulesList = CVarList(tmpRules, 0, ';');

            bool hadWorkspaceRule = false;
            rulesList.map([&](std::string& rule) {
                if (rule.find("workspace") == 0) {
                    rule             = workspaceRule;
                    hadWorkspaceRule = true;
                }
            });

            if (!hadWorkspaceRule)
                rulesList.append(workspaceRule);

            rules = "[" + rulesList.join(";") + "]";
        } else {
            rules = "[" + workspaceRule + "]";
        }

        return std::optional<std::string>(rules + " " + cmd);
    }

    return std::nullopt;
}


bool truthy(const std::string& str) {
    if (str == "1")
        return true;

    std::string cpy = str;
    std::ranges::transform(cpy, cpy.begin(), ::tolower);

    return cpy.starts_with("true") || cpy.starts_with("yes") || cpy.starts_with("on");
}
