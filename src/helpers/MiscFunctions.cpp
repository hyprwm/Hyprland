#include "MiscFunctions.hpp"
#include "../defines.hpp"
#include <algorithm>
#include "../Compositor.hpp"
#include "../managers/TokenManager.hpp"
#include "Monitor.hpp"
#include "../config/ConfigManager.hpp"
#include "fs/FsUtils.hpp"
#include <optional>
#include <cstring>
#include <climits>
#include <cmath>
#include <filesystem>
#include <set>
#include <sys/utsname.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iomanip>
#include <sstream>
#ifdef HAS_EXECINFO
#include <execinfo.h>
#endif
#include <hyprutils/string/String.hpp>
#include <hyprutils/os/Process.hpp>
using namespace Hyprutils::String;
using namespace Hyprutils::OS;

#if defined(__DragonFly__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/sysctl.h>
#if defined(__DragonFly__)
#include <sys/kinfo.h> // struct kinfo_proc
#elif defined(__FreeBSD__)
#include <sys/user.h> // struct kinfo_proc
#endif

#if defined(__NetBSD__)
#undef KERN_PROC
#define KERN_PROC  KERN_PROC2
#define KINFO_PROC struct kinfo_proc2
#else
#define KINFO_PROC struct kinfo_proc
#endif
#if defined(__DragonFly__)
#define KP_PPID(kp) kp.kp_ppid
#elif defined(__FreeBSD__)
#define KP_PPID(kp) kp.ki_ppid
#else
#define KP_PPID(kp) kp.p_ppid
#endif
#endif

std::string absolutePath(const std::string& rawpath, const std::string& currentPath) {
    auto value = rawpath;

    if (value[0] == '~') {
        static const char* const ENVHOME = getenv("HOME");
        value.replace(0, 1, std::string(ENVHOME));
    } else if (value[0] != '/') {
        auto currentDir = currentPath.substr(0, currentPath.find_last_of('/'));

        if (value[0] == '.') {
            if (value[1] == '.' && value[2] == '/') {
                auto parentDir = currentDir.substr(0, currentDir.find_last_of('/'));
                value.replace(0, 2 + currentPath.empty(), parentDir);
            } else if (value[1] == '/')
                value.replace(0, 1 + currentPath.empty(), currentDir);
            else
                value = currentDir + '/' + value;
        } else
            value = currentDir + '/' + value;
    }

    return value;
}

std::string escapeJSONStrings(const std::string& str) {
    std::ostringstream oss;
    for (auto const& c : str) {
        switch (c) {
            case '"': oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\b': oss << "\\b"; break;
            case '\f': oss << "\\f"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default:
                if ('\x00' <= c && c <= '\x1f') {
                    oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << sc<int>(c);
                } else {
                    oss << c;
                }
        }
    }
    return oss.str();
}

std::optional<float> getPlusMinusKeywordResult(std::string source, float relative) {
    try {
        return relative + stof(source);
    } catch (...) {
        Debug::log(ERR, "Invalid arg \"{}\" in getPlusMinusKeywordResult!", source);
        return {};
    }
}

bool isDirection(const std::string& arg) {
    return arg == "l" || arg == "r" || arg == "u" || arg == "d" || arg == "t" || arg == "b";
}

bool isDirection(const char& arg) {
    return arg == 'l' || arg == 'r' || arg == 'u' || arg == 'd' || arg == 't' || arg == 'b';
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
        if ((same_mon || next) && !g_pCompositor->m_lastMonitor) {
            Debug::log(ERR, "Empty monitor workspace on monitor null!");
            return {WORKSPACE_INVALID};
        }

        std::set<WORKSPACEID> invalidWSes;
        if (same_mon) {
            for (auto const& rule : g_pConfigManager->getAllWorkspaceRules()) {
                const auto PMONITOR = g_pCompositor->getMonitorFromString(rule.monitor);
                if (PMONITOR && (PMONITOR->m_id != g_pCompositor->m_lastMonitor->m_id))
                    invalidWSes.insert(rule.workspaceId);
            }
        }

        WORKSPACEID id = next ? g_pCompositor->m_lastMonitor->activeWorkspaceID() : 0;
        while (++id < LONG_MAX) {
            const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(id);
            if (!invalidWSes.contains(id) && (!PWORKSPACE || PWORKSPACE->getWindows() == 0)) {
                result.id = id;
                return result;
            }
        }
    } else if (in.starts_with("prev")) {
        if (!g_pCompositor->m_lastMonitor)
            return {WORKSPACE_INVALID};

        const auto PWORKSPACE = g_pCompositor->m_lastMonitor->m_activeWorkspace;

        if (!valid(PWORKSPACE))
            return {WORKSPACE_INVALID};

        const auto PREVWORKSPACEIDNAME = PWORKSPACE->getPrevWorkspaceIDName();

        if (PREVWORKSPACEIDNAME.id == -1)
            return {WORKSPACE_INVALID};

        const auto PLASTWORKSPACE = g_pCompositor->getWorkspaceByID(PREVWORKSPACEIDNAME.id);

        if (!PLASTWORKSPACE) {
            Debug::log(LOG, "previous workspace {} doesn't exist yet", PREVWORKSPACEIDNAME.id);
            return {PREVWORKSPACEIDNAME.id, PREVWORKSPACEIDNAME.name};
        }

        return {PLASTWORKSPACE->m_id, PLASTWORKSPACE->m_name};
    } else if (in == "next") {
        if (!g_pCompositor->m_lastMonitor || !g_pCompositor->m_lastMonitor->m_activeWorkspace) {
            Debug::log(ERR, "no active monitor or workspace for 'next'");
            return {WORKSPACE_INVALID};
        }

        auto        PCURRENTWORKSPACE = g_pCompositor->m_lastMonitor->m_activeWorkspace;

        WORKSPACEID nextId = PCURRENTWORKSPACE->m_id + 1;

        if (nextId <= 0)
            return {WORKSPACE_INVALID};

        result.id   = nextId;
        result.name = std::to_string(nextId);
        return result;
    } else {
        if (in[0] == 'r' && (in[1] == '-' || in[1] == '+' || in[1] == '~') && isNumber(in.substr(2))) {
            bool absolute = in[1] == '~';
            if (!g_pCompositor->m_lastMonitor) {
                Debug::log(ERR, "Relative monitor workspace on monitor null!");
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
                if (ws->m_isSpecialWorkspace || (ws->m_monitor != g_pCompositor->m_lastMonitor)) {
                    // Can't jump to this workspace
                    invalidWSes.insert(ws->m_id);
                }
            }
            for (auto const& rule : g_pConfigManager->getAllWorkspaceRules()) {
                const auto PMONITOR = g_pCompositor->getMonitorFromString(rule.monitor);
                if (!PMONITOR || PMONITOR->m_id == g_pCompositor->m_lastMonitor->m_id) {
                    // Can't be invalid
                    continue;
                }
                // WS is bound to another monitor, can't jump to this
                invalidWSes.insert(rule.workspaceId);
            }

            // Prepare all named workspaces in case when we need them
            std::vector<WORKSPACEID> namedWSes;
            for (auto const& ws : g_pCompositor->getWorkspaces()) {
                if (ws->m_isSpecialWorkspace || (ws->m_monitor != g_pCompositor->m_lastMonitor) || ws->m_id >= 0)
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
                WORKSPACEID activeWSID    = g_pCompositor->m_lastMonitor->m_activeWorkspace ? g_pCompositor->m_lastMonitor->m_activeWorkspace->m_id : 1;
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

            if (!g_pCompositor->m_lastMonitor) {
                Debug::log(ERR, "Relative monitor workspace on monitor null!");
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
                if (ws->m_isSpecialWorkspace || (ws->m_monitor != g_pCompositor->m_lastMonitor && !onAllMonitors))
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
                WORKSPACEID activeWSID = g_pCompositor->m_lastMonitor->m_activeWorkspace ? g_pCompositor->m_lastMonitor->m_activeWorkspace->m_id : 1;
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
                if (g_pCompositor->m_lastMonitor) {
                    const auto PLUSMINUSRESULT = getPlusMinusKeywordResult(in, g_pCompositor->m_lastMonitor->activeWorkspaceID());
                    if (!PLUSMINUSRESULT.has_value())
                        return {WORKSPACE_INVALID};

                    result.id = std::max(sc<int>(PLUSMINUSRESULT.value()), 1);
                } else {
                    Debug::log(ERR, "Relative workspace on no mon!");
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

    return result;
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

float vecToRectDistanceSquared(const Vector2D& vec, const Vector2D& p1, const Vector2D& p2) {
    const float DX = std::max({0.0, p1.x - vec.x, vec.x - p2.x});
    const float DY = std::max({0.0, p1.y - vec.y, vec.y - p2.y});
    return DX * DX + DY * DY;
}

// Execute a shell command and get the output
std::string execAndGet(const char* cmd) {
    CProcess proc("/bin/sh", {"-c", cmd});

    if (!proc.runSync())
        return "error";

    return proc.stdOut();
}

void logSystemInfo() {
    struct utsname unameInfo;

    uname(&unameInfo);

    Debug::log(LOG, "System name: {}", std::string{unameInfo.sysname});
    Debug::log(LOG, "Node name: {}", std::string{unameInfo.nodename});
    Debug::log(LOG, "Release: {}", std::string{unameInfo.release});
    Debug::log(LOG, "Version: {}", std::string{unameInfo.version});

    Debug::log(NONE, "\n");

#if defined(__DragonFly__) || defined(__FreeBSD__)
    const std::string GPUINFO = execAndGet("pciconf -lv | grep -F -A4 vga");
#elif defined(__arm__) || defined(__aarch64__)
    std::string                 GPUINFO;
    const std::filesystem::path dev_tree = "/proc/device-tree";
    try {
        if (std::filesystem::exists(dev_tree) && std::filesystem::is_directory(dev_tree)) {
            std::for_each(std::filesystem::directory_iterator(dev_tree), std::filesystem::directory_iterator{}, [&](const std::filesystem::directory_entry& entry) {
                if (std::filesystem::is_directory(entry) && entry.path().filename().string().starts_with("soc")) {
                    std::for_each(std::filesystem::directory_iterator(entry.path()), std::filesystem::directory_iterator{}, [&](const std::filesystem::directory_entry& sub_entry) {
                        if (std::filesystem::is_directory(sub_entry) && sub_entry.path().filename().string().starts_with("gpu")) {
                            std::filesystem::path file_path = sub_entry.path() / "compatible";
                            std::ifstream         file(file_path);
                            if (file)
                                GPUINFO.append(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
                        }
                    });
                }
            });
        }
    } catch (...) { GPUINFO = "error"; }
#else
    const std::string GPUINFO = execAndGet("lspci -vnn | grep -E '(VGA|Display|3D)'");
#endif
    Debug::log(LOG, "GPU information:\n{}\n", GPUINFO);

    if (GPUINFO.contains("NVIDIA")) {
        Debug::log(WARN, "Warning: you're using an NVIDIA GPU. Make sure you follow the instructions on the wiki if anything is amiss.\n");
    }

    // log etc
    Debug::log(LOG, "os-release:");

    Debug::log(NONE, "{}", NFsUtils::readFileAsString("/etc/os-release").value_or("error"));
}

int64_t getPPIDof(int64_t pid) {
#if defined(KERN_PROC_PID)
    int mib[] = {
        CTL_KERN,           KERN_PROC, KERN_PROC_PID, (int)pid,
#if defined(__NetBSD__) || defined(__OpenBSD__)
        sizeof(KINFO_PROC), 1,
#endif
    };
    u_int      miblen = sizeof(mib) / sizeof(mib[0]);
    KINFO_PROC kp;
    size_t     sz = sizeof(KINFO_PROC);
    if (sysctl(mib, miblen, &kp, &sz, NULL, 0) != -1)
        return KP_PPID(kp);

    return 0;
#else
    std::string dir = "/proc/" + std::to_string(pid) + "/status";
    FILE*       infile;

    infile = fopen(dir.c_str(), "r");
    if (!infile)
        return 0;

    char*       line = nullptr;
    size_t      len  = 0;
    ssize_t     len2 = 0;

    std::string pidstr;

    while ((len2 = getline(&line, &len, infile)) != -1) {
        if (strstr(line, "PPid:")) {
            pidstr            = std::string(line, len2);
            const auto tabpos = pidstr.find_last_of('\t');
            if (tabpos != std::string::npos)
                pidstr = pidstr.substr(tabpos);
            break;
        }
    }

    fclose(infile);
    if (line)
        free(line);

    try {
        return std::stoll(pidstr);
    } catch (std::exception& e) { return 0; }
#endif
}

std::expected<int64_t, std::string> configStringToInt(const std::string& VALUE) {
    auto parseHex = [](const std::string& value) -> std::expected<int64_t, std::string> {
        try {
            size_t position;
            auto   result = stoll(value, &position, 16);
            if (position == value.size())
                return result;
        } catch (const std::exception&) {}
        return std::unexpected("invalid hex " + value);
    };
    if (VALUE.starts_with("0x")) {
        // Values with 0x are hex
        return parseHex(VALUE);
    } else if (VALUE.starts_with("rgba(") && VALUE.ends_with(')')) {
        const auto VALUEWITHOUTFUNC = trim(VALUE.substr(5, VALUE.length() - 6));

        // try doing it the comma way first
        if (std::ranges::count(VALUEWITHOUTFUNC, ',') == 3) {
            // cool
            std::string rolling = VALUEWITHOUTFUNC;
            auto        r       = configStringToInt(trim(rolling.substr(0, rolling.find(','))));
            rolling             = rolling.substr(rolling.find(',') + 1);
            auto g              = configStringToInt(trim(rolling.substr(0, rolling.find(','))));
            rolling             = rolling.substr(rolling.find(',') + 1);
            auto b              = configStringToInt(trim(rolling.substr(0, rolling.find(','))));
            rolling             = rolling.substr(rolling.find(',') + 1);
            uint8_t a           = 0;

            if (!r || !g || !b)
                return std::unexpected("failed parsing " + VALUEWITHOUTFUNC);

            try {
                a = std::round(std::stof(trim(rolling.substr(0, rolling.find(',')))) * 255.f);
            } catch (std::exception& e) { return std::unexpected("failed parsing " + VALUEWITHOUTFUNC); }

            return a * sc<Hyprlang::INT>(0x1000000) + *r * sc<Hyprlang::INT>(0x10000) + *g * sc<Hyprlang::INT>(0x100) + *b;
        } else if (VALUEWITHOUTFUNC.length() == 8) {
            const auto RGBA = parseHex(VALUEWITHOUTFUNC);

            if (!RGBA)
                return RGBA;
            // now we need to RGBA -> ARGB. The config holds ARGB only.
            return (*RGBA >> 8) + 0x1000000 * (*RGBA & 0xFF);
        }

        return std::unexpected("rgba() expects length of 8 characters (4 bytes) or 4 comma separated values");

    } else if (VALUE.starts_with("rgb(") && VALUE.ends_with(')')) {
        const auto VALUEWITHOUTFUNC = trim(VALUE.substr(4, VALUE.length() - 5));

        // try doing it the comma way first
        if (std::ranges::count(VALUEWITHOUTFUNC, ',') == 2) {
            // cool
            std::string rolling = VALUEWITHOUTFUNC;
            auto        r       = configStringToInt(trim(rolling.substr(0, rolling.find(','))));
            rolling             = rolling.substr(rolling.find(',') + 1);
            auto g              = configStringToInt(trim(rolling.substr(0, rolling.find(','))));
            rolling             = rolling.substr(rolling.find(',') + 1);
            auto b              = configStringToInt(trim(rolling.substr(0, rolling.find(','))));

            if (!r || !g || !b)
                return std::unexpected("failed parsing " + VALUEWITHOUTFUNC);

            return sc<Hyprlang::INT>(0xFF000000) + *r * sc<Hyprlang::INT>(0x10000) + *g * sc<Hyprlang::INT>(0x100) + *b;
        } else if (VALUEWITHOUTFUNC.length() == 6) {
            auto r = parseHex(VALUEWITHOUTFUNC);
            return r ? *r + 0xFF000000 : r;
        }

        return std::unexpected("rgb() expects length of 6 characters (3 bytes) or 3 comma separated values");
    } else if (VALUE.starts_with("true") || VALUE.starts_with("on") || VALUE.starts_with("yes")) {
        return 1;
    } else if (VALUE.starts_with("false") || VALUE.starts_with("off") || VALUE.starts_with("no")) {
        return 0;
    }

    if (VALUE.empty() || !isNumber(VALUE, false))
        return std::unexpected("cannot parse \"" + VALUE + "\" as an int.");

    try {
        const auto RES = std::stoll(VALUE);
        return RES;
    } catch (std::exception& e) { return std::unexpected(std::string{"stoll threw: "} + e.what()); }

    return std::unexpected("parse error");
}

Vector2D configStringToVector2D(const std::string& VALUE) {
    std::istringstream iss(VALUE);
    std::string        token;

    if (!std::getline(iss, token, ' ') && !std::getline(iss, token, ','))
        throw std::invalid_argument("Invalid string format");

    if (!isNumber(token))
        throw std::invalid_argument("Invalid x value");

    long long x = std::stoll(token);

    if (!std::getline(iss, token))
        throw std::invalid_argument("Invalid string format");

    if (!isNumber(token))
        throw std::invalid_argument("Invalid y value");

    long long y = std::stoll(token);

    if (std::getline(iss, token))
        throw std::invalid_argument("Invalid string format");

    return Vector2D(sc<double>(x), sc<double>(y));
}

double normalizeAngleRad(double ang) {
    if (ang > M_PI * 2) {
        while (ang > M_PI * 2)
            ang -= M_PI * 2;
        return ang;
    }

    if (ang < 0.0) {
        while (ang < 0.0)
            ang += M_PI * 2;
        return ang;
    }

    return ang;
}

std::vector<SCallstackFrameInfo> getBacktrace() {
    std::vector<SCallstackFrameInfo> callstack;

#ifdef HAS_EXECINFO
    void*  bt[1024];
    int    btSize;
    char** btSymbols;

    btSize    = backtrace(bt, 1024);
    btSymbols = backtrace_symbols(bt, btSize);

    for (auto i = 0; i < btSize; ++i) {
        callstack.emplace_back(SCallstackFrameInfo{bt[i], std::string{btSymbols[i]}});
    }
#else
    callstack.emplace_back(SCallstackFrameInfo{nullptr, "configuration does not support execinfo.h"});
#endif

    return callstack;
}

void throwError(const std::string& err) {
    Debug::log(CRIT, "Critical error thrown: {}", err);
    throw std::runtime_error(err);
}

bool envEnabled(const std::string& env) {
    const auto ENV = getenv(env.c_str());
    if (!ENV)
        return false;
    return std::string(ENV) == "1";
}

std::pair<CFileDescriptor, std::string> openExclusiveShm() {
    // Only absolute paths can be shared across different shm_open() calls
    std::string name = "/" + g_pTokenManager->getRandomUUID();

    for (size_t i = 0; i < 69; ++i) {
        CFileDescriptor fd{shm_open(name.c_str(), O_RDWR | O_CREAT | O_EXCL, 0600)};
        if (fd.isValid())
            return {std::move(fd), name};
    }

    return {{}, ""};
}

CFileDescriptor allocateSHMFile(size_t len) {
    auto [fd, name] = openExclusiveShm();
    if (!fd.isValid())
        return {};

    shm_unlink(name.c_str());

    int ret;
    do {
        ret = ftruncate(fd.get(), len);
    } while (ret < 0 && errno == EINTR);

    if (ret < 0) {
        return {};
    }

    return std::move(fd);
}

bool allocateSHMFilePair(size_t size, CFileDescriptor& rw_fd_ptr, CFileDescriptor& ro_fd_ptr) {
    auto [fd, name] = openExclusiveShm();
    if (!fd.isValid()) {
        return false;
    }

    // CLOEXEC is guaranteed to be set by shm_open
    CFileDescriptor ro_fd{shm_open(name.c_str(), O_RDONLY, 0)};
    if (!ro_fd.isValid()) {
        shm_unlink(name.c_str());
        return false;
    }

    shm_unlink(name.c_str());

    // Make sure the file cannot be re-opened in read-write mode (e.g. via
    // "/proc/self/fd/" on Linux)
    if (fchmod(fd.get(), 0) != 0) {
        return false;
    }

    int ret;
    do {
        ret = ftruncate(fd.get(), size);
    } while (ret < 0 && errno == EINTR);
    if (ret < 0) {
        return false;
    }

    rw_fd_ptr = std::move(fd);
    ro_fd_ptr = std::move(ro_fd);
    return true;
}

float stringToPercentage(const std::string& VALUE, const float REL) {
    if (VALUE.ends_with('%'))
        return (std::stof(VALUE.substr(0, VALUE.length() - 1)) * REL) / 100.f;
    else
        return std::stof(VALUE);
}

// Checks if Nvidia driver major version is at least given version.
// Useful for explicit_sync_kms and ctm_animation as they only work
// past certain driver versions.
bool isNvidiaDriverVersionAtLeast(int threshold) {
    static int  driverMajor = 0;
    static bool once        = true;

    if (once) {
        once = false;

        std::error_code ec;
        if (std::filesystem::exists("/sys/module/nvidia_drm/version", ec) && !ec) {
            std::ifstream ifs("/sys/module/nvidia_drm/version");
            if (ifs.good()) {
                try {
                    std::string driverInfo((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));

                    size_t      firstDot = driverInfo.find('.');
                    if (firstDot != std::string::npos)
                        driverMajor = std::stoi(driverInfo.substr(0, firstDot));

                    Debug::log(LOG, "Parsed NVIDIA major version: {}", driverMajor);

                } catch (std::exception& e) {
                    driverMajor = 0; // Default to 0 if parsing fails
                }

                ifs.close();
            }
        }
    }

    return driverMajor >= threshold;
}

std::expected<std::string, std::string> binaryNameForWlClient(wl_client* client) {
    if (!client)
        return std::unexpected("client unknown");

    pid_t pid = 0;
    wl_client_get_credentials(client, &pid, nullptr, nullptr);

    return binaryNameForPid(pid);
}

std::expected<std::string, std::string> binaryNameForPid(pid_t pid) {
    if (pid <= 0)
        return std::unexpected("No pid for client");

#if defined(KERN_PROC_PATHNAME)
    int mib[] = {
        CTL_KERN,
#if defined(__NetBSD__)
        KERN_PROC_ARGS,
        pid,
        KERN_PROC_PATHNAME,
#else
        KERN_PROC,
        KERN_PROC_PATHNAME,
        pid,
#endif
    };
    u_int  miblen        = sizeof(mib) / sizeof(mib[0]);
    char   exe[PATH_MAX] = "/nonexistent";
    size_t sz            = sizeof(exe);
    sysctl(mib, miblen, &exe, &sz, NULL, 0);
    std::string path = exe;
#else
    std::string path = std::format("/proc/{}/exe", sc<uint64_t>(pid));
#endif
    std::error_code ec;

    std::string     fullPath = std::filesystem::canonical(path, ec);

    if (ec)
        return std::unexpected("canonical failed");

    return fullPath;
}

std::string deviceNameToInternalString(std::string in) {
    std::ranges::replace(in, ' ', '-');
    std::ranges::replace(in, '\n', '-');
    std::ranges::replace(in, ',', '-');
    std::ranges::transform(in, in.begin(), ::tolower);
    return in;
}
