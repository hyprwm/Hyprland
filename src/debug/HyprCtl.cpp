#include "HyprCtl.hpp"
#include "helpers/Monitor.hpp"

#include <algorithm>
#include <format>
#include <fstream>
#include <iterator>
#include <netinet/in.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/un.h>
#include <unistd.h>
#include <sys/poll.h>
#include <filesystem>
#include <ranges>
#include <sys/eventfd.h>

#include <sstream>
#include <string>
#include <typeindex>
#include <numeric>

#include <hyprutils/string/String.hpp>
#include <hyprutils/os/FileDescriptor.hpp>
using namespace Hyprutils::String;
using namespace Hyprutils::OS;
#include <aquamarine/input/Input.hpp>

#include "../config/ConfigDataValues.hpp"
#include "../config/ConfigValue.hpp"
#include "../managers/CursorManager.hpp"
#include "../hyprerror/HyprError.hpp"
#include "../devices/IPointer.hpp"
#include "../devices/IKeyboard.hpp"
#include "../devices/ITouch.hpp"
#include "../devices/Tablet.hpp"
#include "../protocols/GlobalShortcuts.hpp"
#include "debug/RollingLogFollow.hpp"
#include "config/ConfigManager.hpp"
#include "helpers/MiscFunctions.hpp"
#include "../desktop/LayerSurface.hpp"
#include "../version.h"

#include "../Compositor.hpp"
#include "../managers/input/InputManager.hpp"
#include "../managers/XWaylandManager.hpp"
#include "../managers/LayoutManager.hpp"
#include "../plugins/PluginSystem.hpp"
#include "../managers/animation/AnimationManager.hpp"
#include "../debug/HyprNotificationOverlay.hpp"
#include "../render/Renderer.hpp"
#include "../render/OpenGL.hpp"

#if defined(__DragonFly__) || defined(__FreeBSD__)
#include <sys/ucred.h>
#define CRED_T   xucred
#define CRED_LVL SOL_LOCAL
#define CRED_OPT LOCAL_PEERCRED
#define CRED_PID cr_pid
#elif defined(__NetBSD__)
#define CRED_T   unpcbid
#define CRED_LVL SOL_LOCAL
#define CRED_OPT LOCAL_PEEREID
#define CRED_PID unp_pid
#else
#if defined(__OpenBSD__)
#define CRED_T sockpeercred
#else
#define CRED_T ucred
#endif
#define CRED_LVL SOL_SOCKET
#define CRED_OPT SO_PEERCRED
#define CRED_PID pid
#endif

static void trimTrailingComma(std::string& str) {
    if (!str.empty() && str.back() == ',')
        str.pop_back();
}

static std::string formatToString(uint32_t drmFormat) {
    switch (drmFormat) {
        case DRM_FORMAT_XRGB2101010: return "XRGB2101010";
        case DRM_FORMAT_XBGR2101010: return "XBGR2101010";
        case DRM_FORMAT_XRGB8888: return "XRGB8888";
        case DRM_FORMAT_XBGR8888: return "XBGR8888";
        default: break;
    }

    return "Invalid";
}

static std::string availableModesForOutput(PHLMONITOR pMonitor, eHyprCtlOutputFormat format) {
    std::string result;

    for (auto const& m : pMonitor->m_output->modes) {
        if (format == FORMAT_NORMAL)
            result += std::format("{}x{}@{:.2f}Hz ", m->pixelSize.x, m->pixelSize.y, m->refreshRate / 1000.0);
        else
            result += std::format("\"{}x{}@{:.2f}Hz\",", m->pixelSize.x, m->pixelSize.y, m->refreshRate / 1000.0);
    }

    trimTrailingComma(result);

    return result;
}

const std::array<const char*, CMonitor::SC_CHECKS_COUNT> SOLITARY_REASONS_JSON = {
    "\"UNKNOWN\"",   "\"NOTIFICATION\"", "\"LOCK\"",      "\"WORKSPACE\"", "\"WINDOWED\"", "\"DND\"",        "\"SPECIAL\"",  "\"ALPHA\"",       "\"OFFSET\"",
    "\"CANDIDATE\"", "\"OPAQUE\"",       "\"TRANSFORM\"", "\"OVERLAYS\"",  "\"FLOAT\"",    "\"WORKSPACES\"", "\"SURFACES\"", "\"CONFIGERROR\"",
};

const std::array<const char*, CMonitor::SC_CHECKS_COUNT> SOLITARY_REASONS_TEXT = {
    "unknown reason",    "notification",     "session lock",     "invalid workspace", "windowed mode", "dnd active",
    "special workspace", "alpha channel",    "workspace offset", "missing candidate", "not opaque",    "surface transformations",
    "other overlays",    "floating windows", "other workspaces", "subsurfaces",       "config error",
};

std::string CHyprCtl::getSolitaryBlockedReason(Hyprutils::Memory::CSharedPointer<CMonitor> m, eHyprCtlOutputFormat format) {
    const auto reasons = m->isSolitaryBlocked(true);
    if (!reasons)
        return "null";

    std::string reasonStr = "";
    const auto  TEXTS     = format == eHyprCtlOutputFormat::FORMAT_JSON ? SOLITARY_REASONS_JSON : SOLITARY_REASONS_TEXT;

    for (uint32_t i = 0; i < CMonitor::SC_CHECKS_COUNT; i++) {
        if (reasons & (1 << i)) {
            if (reasonStr != "")
                reasonStr += ",";
            reasonStr += TEXTS[i];
        }
    }

    return format == eHyprCtlOutputFormat::FORMAT_JSON ? "[" + reasonStr + "]" : reasonStr;
}

const std::array<const char*, CMonitor::DS_CHECKS_COUNT> DS_REASONS_JSON = {
    "\"UNKNOWN\"",   "\"USER\"",    "\"WINDOWED\"",  "\"CONTENT\"", "\"MIRROR\"",  "\"RECORD\"", "\"SW\"",
    "\"CANDIDATE\"", "\"SURFACE\"", "\"TRANSFORM\"", "\"DMA\"",     "\"TEARING\"", "\"FAILED\"", "\"CM\"",
};

const std::array<const char*, CMonitor::DS_CHECKS_COUNT> DS_REASONS_TEXT = {
    "unknown reason",    "user settings",   "windowed mode",           "content type",   "monitor mirrors", "screen record/screenshot", "software renders/cursors",
    "missing candidate", "invalid surface", "surface transformations", "invalid buffer", "tearing",         "activation failed",        "color management",
};

std::string CHyprCtl::getDSBlockedReason(Hyprutils::Memory::CSharedPointer<CMonitor> m, eHyprCtlOutputFormat format) {
    const auto reasons = m->isDSBlocked(true);
    if (!reasons)
        return "null";

    std::string reasonStr = "";
    const auto  TEXTS     = format == eHyprCtlOutputFormat::FORMAT_JSON ? DS_REASONS_JSON : DS_REASONS_TEXT;

    for (int i = 0; i < CMonitor::DS_CHECKS_COUNT; i++) {
        if (reasons & (1 << i)) {
            if (reasonStr != "")
                reasonStr += ",";
            reasonStr += TEXTS[i];
        }
    }

    return format == eHyprCtlOutputFormat::FORMAT_JSON ? "[" + reasonStr + "]" : reasonStr;
}

const std::array<const char*, CMonitor::TC_CHECKS_COUNT> TEARING_REASONS_JSON = {
    "\"UNKNOWN\"", "\"NOT_TORN\"", "\"USER\"", "\"ZOOM\"", "\"SUPPORT\"", "\"CANDIDATE\"", "\"WINDOW\"",
};

const std::array<const char*, CMonitor::TC_CHECKS_COUNT> TEARING_REASONS_TEXT = {
    "unknown reason", "next frame is not torn", "user settings", "zoom", "not supported by monitor", "missing candidate", "window settings",
};

std::string CHyprCtl::getTearingBlockedReason(Hyprutils::Memory::CSharedPointer<CMonitor> m, eHyprCtlOutputFormat format) {
    const auto reasons = m->isTearingBlocked(true);
    if (!reasons || (reasons == CMonitor::TC_NOT_TORN && m->m_tearingState.activelyTearing))
        return "null";

    std::string reasonStr = "";
    const auto  TEXTS     = format == eHyprCtlOutputFormat::FORMAT_JSON ? TEARING_REASONS_JSON : TEARING_REASONS_TEXT;

    for (int i = 0; i < CMonitor::TC_CHECKS_COUNT; i++) {
        if (reasons & (1 << i)) {
            if (reasonStr != "")
                reasonStr += ",";
            reasonStr += TEXTS[i];
        }
    }

    return format == eHyprCtlOutputFormat::FORMAT_JSON ? "[" + reasonStr + "]" : reasonStr;
}

std::string CHyprCtl::getMonitorData(Hyprutils::Memory::CSharedPointer<CMonitor> m, eHyprCtlOutputFormat format) {
    std::string result;
    if (!m->m_output || m->m_id == -1)
        return "";

    if (format == eHyprCtlOutputFormat::FORMAT_JSON) {

        result += std::format(
            R"#({{
    "id": {},
    "name": "{}",
    "description": "{}",
    "make": "{}",
    "model": "{}",
    "serial": "{}",
    "width": {},
    "height": {},
    "physicalWidth": {},
    "physicalHeight": {},
    "refreshRate": {:.5f},
    "x": {},
    "y": {},
    "activeWorkspace": {{
        "id": {},
        "name": "{}"
    }},
    "specialWorkspace": {{
        "id": {},
        "name": "{}"
    }},
    "reserved": [{}, {}, {}, {}],
    "scale": {:.2f},
    "transform": {},
    "focused": {},
    "dpmsStatus": {},
    "vrr": {},
    "solitary": "{:x}",
    "solitaryBlockedBy": {},
    "activelyTearing": {},
    "tearingBlockedBy": {},
    "directScanoutTo": "{:x}",
    "directScanoutBlockedBy": {},
    "disabled": {},
    "currentFormat": "{}",
    "mirrorOf": "{}",
    "availableModes": [{}]
}},)#",

            m->m_id, escapeJSONStrings(m->m_name), escapeJSONStrings(m->m_shortDescription), escapeJSONStrings(m->m_output->make), escapeJSONStrings(m->m_output->model),
            escapeJSONStrings(m->m_output->serial), sc<int>(m->m_pixelSize.x), sc<int>(m->m_pixelSize.y), sc<int>(m->m_output->physicalSize.x),
            sc<int>(m->m_output->physicalSize.y), m->m_refreshRate, sc<int>(m->m_position.x), sc<int>(m->m_position.y), m->activeWorkspaceID(),
            (!m->m_activeWorkspace ? "" : escapeJSONStrings(m->m_activeWorkspace->m_name)), m->activeSpecialWorkspaceID(),
            escapeJSONStrings(m->m_activeSpecialWorkspace ? m->m_activeSpecialWorkspace->m_name : ""), sc<int>(m->m_reservedTopLeft.x), sc<int>(m->m_reservedTopLeft.y),
            sc<int>(m->m_reservedBottomRight.x), sc<int>(m->m_reservedBottomRight.y), m->m_scale, sc<int>(m->m_transform), (m == g_pCompositor->m_lastMonitor ? "true" : "false"),
            (m->m_dpmsStatus ? "true" : "false"), (m->m_output->state->state().adaptiveSync ? "true" : "false"), rc<uint64_t>(m->m_solitaryClient.get()),
            getSolitaryBlockedReason(m, format), (m->m_tearingState.activelyTearing ? "true" : "false"), getTearingBlockedReason(m, format), rc<uint64_t>(m->m_lastScanout.get()),
            getDSBlockedReason(m, format), (m->m_enabled ? "false" : "true"), formatToString(m->m_output->state->state().drmFormat),
            m->m_mirrorOf ? std::format("{}", m->m_mirrorOf->m_id) : "none", availableModesForOutput(m, format));

    } else {
        result += std::format(
            "Monitor {} (ID {}):\n\t{}x{}@{:.5f} at {}x{}\n\tdescription: {}\n\tmake: {}\n\tmodel: {}\n\tphysical size (mm): {}x{}\n\tserial: {}\n\tactive workspace: {} ({})\n\t"
            "special workspace: {} ({})\n\treserved: {} {} {} {}\n\tscale: {:.2f}\n\ttransform: {}\n\tfocused: {}\n\t"
            "dpmsStatus: {}\n\tvrr: {}\n\tsolitary: {:x}\n\tsolitaryBlockedBy: {}\n\tactivelyTearing: {}\n\ttearingBlockedBy: {}\n\tdirectScanoutTo: "
            "{:x}\n\tdirectScanoutBlockedBy: {}\n\tdisabled: "
            "{}\n\tcurrentFormat: {}\n\tmirrorOf: "
            "{}\n\tavailableModes: {}\n\n",
            m->m_name, m->m_id, sc<int>(m->m_pixelSize.x), sc<int>(m->m_pixelSize.y), m->m_refreshRate, sc<int>(m->m_position.x), sc<int>(m->m_position.y), m->m_shortDescription,
            m->m_output->make, m->m_output->model, sc<int>(m->m_output->physicalSize.x), sc<int>(m->m_output->physicalSize.y), m->m_output->serial, m->activeWorkspaceID(),
            (!m->m_activeWorkspace ? "" : m->m_activeWorkspace->m_name), m->activeSpecialWorkspaceID(), (m->m_activeSpecialWorkspace ? m->m_activeSpecialWorkspace->m_name : ""),
            sc<int>(m->m_reservedTopLeft.x), sc<int>(m->m_reservedTopLeft.y), sc<int>(m->m_reservedBottomRight.x), sc<int>(m->m_reservedBottomRight.y), m->m_scale,
            sc<int>(m->m_transform), (m == g_pCompositor->m_lastMonitor ? "yes" : "no"), sc<int>(m->m_dpmsStatus), m->m_output->state->state().adaptiveSync,
            rc<uint64_t>(m->m_solitaryClient.get()), getSolitaryBlockedReason(m, format), m->m_tearingState.activelyTearing, getTearingBlockedReason(m, format),
            rc<uint64_t>(m->m_lastScanout.get()), getDSBlockedReason(m, format), !m->m_enabled, formatToString(m->m_output->state->state().drmFormat),
            m->m_mirrorOf ? std::format("{}", m->m_mirrorOf->m_id) : "none", availableModesForOutput(m, format));
    }

    return result;
}

static std::string monitorsRequest(eHyprCtlOutputFormat format, std::string request) {
    CVarList vars(request, 0, ' ');
    auto     allMonitors = false;

    if (vars.size() > 2)
        return "too many args";

    if (vars.size() == 2 && vars[1] == "all")
        allMonitors = true;

    std::string result = "";
    if (format == eHyprCtlOutputFormat::FORMAT_JSON) {
        result += "[";

        for (auto const& m : allMonitors ? g_pCompositor->m_realMonitors : g_pCompositor->m_monitors) {
            result += CHyprCtl::getMonitorData(m, format);
        }

        trimTrailingComma(result);

        result += "]";
    } else {
        for (auto const& m : allMonitors ? g_pCompositor->m_realMonitors : g_pCompositor->m_monitors) {
            if (!m->m_output || m->m_id == -1)
                continue;

            result += CHyprCtl::getMonitorData(m, format);
        }
    }

    return result;
}

static std::string getTagsData(PHLWINDOW w, eHyprCtlOutputFormat format) {
    const auto tags = w->m_tags.getTags();

    if (format == eHyprCtlOutputFormat::FORMAT_JSON)
        return std::ranges::fold_left(tags, std::string(),
                                      [](const std::string& a, const std::string& b) { return a.empty() ? std::format("\"{}\"", b) : std::format("{}, \"{}\"", a, b); });
    else
        return std::ranges::fold_left(tags, std::string(), [](const std::string& a, const std::string& b) { return a.empty() ? b : a + ", " + b; });
}

static std::string getGroupedData(PHLWINDOW w, eHyprCtlOutputFormat format) {
    const bool isJson = format == eHyprCtlOutputFormat::FORMAT_JSON;
    if (w->m_groupData.pNextWindow.expired())
        return isJson ? "" : "0";

    std::ostringstream result;

    PHLWINDOW          head = w->getGroupHead();
    PHLWINDOW          curr = head;
    while (true) {
        if (isJson)
            result << std::format("\"0x{:x}\"", rc<uintptr_t>(curr.get()));
        else
            result << std::format("{:x}", rc<uintptr_t>(curr.get()));
        curr = curr->m_groupData.pNextWindow.lock();
        // We've wrapped around to the start, break out without trailing comma
        if (curr == head)
            break;
        result << (isJson ? ", " : ",");
    }

    return result.str();
}

std::string CHyprCtl::getWindowData(PHLWINDOW w, eHyprCtlOutputFormat format) {
    auto getFocusHistoryID = [](PHLWINDOW wnd) -> int {
        for (size_t i = 0; i < g_pCompositor->m_windowFocusHistory.size(); ++i) {
            if (g_pCompositor->m_windowFocusHistory[i].lock() == wnd)
                return i;
        }
        return -1;
    };

    if (format == eHyprCtlOutputFormat::FORMAT_JSON) {
        return std::format(
            R"#({{
    "address": "0x{:x}",
    "mapped": {},
    "hidden": {},
    "at": [{}, {}],
    "size": [{}, {}],
    "workspace": {{
        "id": {},
        "name": "{}"
    }},
    "floating": {},
    "pseudo": {},
    "monitor": {},
    "class": "{}",
    "title": "{}",
    "initialClass": "{}",
    "initialTitle": "{}",
    "pid": {},
    "xwayland": {},
    "pinned": {},
    "fullscreen": {},
    "fullscreenClient": {},
    "grouped": [{}],
    "tags": [{}],
    "swallowing": "0x{:x}",
    "focusHistoryID": {},
    "inhibitingIdle": {},
    "xdgTag": "{}",
    "xdgDescription": "{}"
}},)#",
            rc<uintptr_t>(w.get()), (w->m_isMapped ? "true" : "false"), (w->isHidden() ? "true" : "false"), sc<int>(w->m_realPosition->goal().x),
            sc<int>(w->m_realPosition->goal().y), sc<int>(w->m_realSize->goal().x), sc<int>(w->m_realSize->goal().y), w->m_workspace ? w->workspaceID() : WORKSPACE_INVALID,
            escapeJSONStrings(!w->m_workspace ? "" : w->m_workspace->m_name), (sc<int>(w->m_isFloating) == 1 ? "true" : "false"), (w->m_isPseudotiled ? "true" : "false"),
            w->monitorID(), escapeJSONStrings(w->m_class), escapeJSONStrings(w->m_title), escapeJSONStrings(w->m_initialClass), escapeJSONStrings(w->m_initialTitle), w->getPID(),
            (sc<int>(w->m_isX11) == 1 ? "true" : "false"), (w->m_pinned ? "true" : "false"), sc<uint8_t>(w->m_fullscreenState.internal), sc<uint8_t>(w->m_fullscreenState.client),
            getGroupedData(w, format), getTagsData(w, format), rc<uintptr_t>(w->m_swallowed.get()), getFocusHistoryID(w),
            (g_pInputManager->isWindowInhibiting(w, false) ? "true" : "false"), escapeJSONStrings(w->xdgTag().value_or("")), escapeJSONStrings(w->xdgDescription().value_or("")));
    } else {
        return std::format(
            "Window {:x} -> {}:\n\tmapped: {}\n\thidden: {}\n\tat: {},{}\n\tsize: {},{}\n\tworkspace: {} ({})\n\tfloating: {}\n\tpseudo: {}\n\tmonitor: {}\n\tclass: {}\n\ttitle: "
            "{}\n\tinitialClass: {}\n\tinitialTitle: {}\n\tpid: "
            "{}\n\txwayland: {}\n\tpinned: "
            "{}\n\tfullscreen: {}\n\tfullscreenClient: {}\n\tgrouped: {}\n\ttags: {}\n\tswallowing: {:x}\n\tfocusHistoryID: {}\n\tinhibitingIdle: {}\n\txdgTag: "
            "{}\n\txdgDescription: {}\n\n",
            rc<uintptr_t>(w.get()), w->m_title, sc<int>(w->m_isMapped), sc<int>(w->isHidden()), sc<int>(w->m_realPosition->goal().x), sc<int>(w->m_realPosition->goal().y),
            sc<int>(w->m_realSize->goal().x), sc<int>(w->m_realSize->goal().y), w->m_workspace ? w->workspaceID() : WORKSPACE_INVALID,
            (!w->m_workspace ? "" : w->m_workspace->m_name), sc<int>(w->m_isFloating), sc<int>(w->m_isPseudotiled), w->monitorID(), w->m_class, w->m_title, w->m_initialClass,
            w->m_initialTitle, w->getPID(), sc<int>(w->m_isX11), sc<int>(w->m_pinned), sc<uint8_t>(w->m_fullscreenState.internal), sc<uint8_t>(w->m_fullscreenState.client),
            getGroupedData(w, format), getTagsData(w, format), rc<uintptr_t>(w->m_swallowed.get()), getFocusHistoryID(w), sc<int>(g_pInputManager->isWindowInhibiting(w, false)),
            w->xdgTag().value_or(""), w->xdgDescription().value_or(""));
    }
}

static std::string clientsRequest(eHyprCtlOutputFormat format, std::string request) {
    std::string result = "";
    if (format == eHyprCtlOutputFormat::FORMAT_JSON) {
        result += "[";

        for (auto const& w : g_pCompositor->m_windows) {
            if (!w->m_isMapped && !g_pHyprCtl->m_currentRequestParams.all)
                continue;

            result += CHyprCtl::getWindowData(w, format);
        }

        trimTrailingComma(result);

        result += "]";
    } else {
        for (auto const& w : g_pCompositor->m_windows) {
            if (!w->m_isMapped && !g_pHyprCtl->m_currentRequestParams.all)
                continue;

            result += CHyprCtl::getWindowData(w, format);
        }

        if (result.empty())
            return "no open windows";
    }
    return result;
}

std::string CHyprCtl::getWorkspaceData(PHLWORKSPACE w, eHyprCtlOutputFormat format) {
    const auto PLASTW   = w->getLastFocusedWindow();
    const auto PMONITOR = w->m_monitor.lock();
    if (format == eHyprCtlOutputFormat::FORMAT_JSON) {
        return std::format(R"#({{
    "id": {},
    "name": "{}",
    "monitor": "{}",
    "monitorID": {},
    "windows": {},
    "hasfullscreen": {},
    "lastwindow": "0x{:x}",
    "lastwindowtitle": "{}",
    "ispersistent": {}
}})#",
                           w->m_id, escapeJSONStrings(w->m_name), escapeJSONStrings(PMONITOR ? PMONITOR->m_name : "?"),
                           escapeJSONStrings(PMONITOR ? std::to_string(PMONITOR->m_id) : "null"), w->getWindows(), w->m_hasFullscreenWindow ? "true" : "false",
                           rc<uintptr_t>(PLASTW.get()), PLASTW ? escapeJSONStrings(PLASTW->m_title) : "", w->isPersistent() ? "true" : "false");
    } else {
        return std::format(
            "workspace ID {} ({}) on monitor {}:\n\tmonitorID: {}\n\twindows: {}\n\thasfullscreen: {}\n\tlastwindow: 0x{:x}\n\tlastwindowtitle: {}\n\tispersistent: {}\n\n",
            w->m_id, w->m_name, PMONITOR ? PMONITOR->m_name : "?", PMONITOR ? std::to_string(PMONITOR->m_id) : "null", w->getWindows(), sc<int>(w->m_hasFullscreenWindow),
            rc<uintptr_t>(PLASTW.get()), PLASTW ? PLASTW->m_title : "", sc<int>(w->isPersistent()));
    }
}

static std::string getWorkspaceRuleData(const SWorkspaceRule& r, eHyprCtlOutputFormat format) {
    const auto boolToString = [](const bool b) -> std::string { return b ? "true" : "false"; };
    if (format == eHyprCtlOutputFormat::FORMAT_JSON) {
        const std::string monitor     = r.monitor.empty() ? "" : std::format(",\n    \"monitor\": \"{}\"", escapeJSONStrings(r.monitor));
        const std::string default_    = sc<bool>(r.isDefault) ? std::format(",\n    \"default\": {}", boolToString(r.isDefault)) : "";
        const std::string persistent  = sc<bool>(r.isPersistent) ? std::format(",\n    \"persistent\": {}", boolToString(r.isPersistent)) : "";
        const std::string gapsIn      = sc<bool>(r.gapsIn) ?
                 std::format(",\n    \"gapsIn\": [{}, {}, {}, {}]", r.gapsIn.value().m_top, r.gapsIn.value().m_right, r.gapsIn.value().m_bottom, r.gapsIn.value().m_left) :
                 "";
        const std::string gapsOut     = sc<bool>(r.gapsOut) ?
                std::format(",\n    \"gapsOut\": [{}, {}, {}, {}]", r.gapsOut.value().m_top, r.gapsOut.value().m_right, r.gapsOut.value().m_bottom, r.gapsOut.value().m_left) :
                "";
        const std::string borderSize  = sc<bool>(r.borderSize) ? std::format(",\n    \"borderSize\": {}", r.borderSize.value()) : "";
        const std::string border      = sc<bool>(r.noBorder) ? std::format(",\n    \"border\": {}", boolToString(!r.noBorder.value())) : "";
        const std::string rounding    = sc<bool>(r.noRounding) ? std::format(",\n    \"rounding\": {}", boolToString(!r.noRounding.value())) : "";
        const std::string decorate    = sc<bool>(r.decorate) ? std::format(",\n    \"decorate\": {}", boolToString(r.decorate.value())) : "";
        const std::string shadow      = sc<bool>(r.noShadow) ? std::format(",\n    \"shadow\": {}", boolToString(!r.noShadow.value())) : "";
        const std::string defaultName = r.defaultName.has_value() ? std::format(",\n    \"defaultName\": \"{}\"", escapeJSONStrings(r.defaultName.value())) : "";

        std::string       result =
            std::format(R"#({{
    "workspaceString": "{}"{}{}{}{}{}{}{}{}{}{}{}
}})#",
                        escapeJSONStrings(r.workspaceString), monitor, default_, persistent, gapsIn, gapsOut, borderSize, border, rounding, decorate, shadow, defaultName);

        return result;
    } else {
        const std::string monitor    = std::format("\tmonitor: {}\n", r.monitor.empty() ? "<unset>" : escapeJSONStrings(r.monitor));
        const std::string default_   = std::format("\tdefault: {}\n", sc<bool>(r.isDefault) ? boolToString(r.isDefault) : "<unset>");
        const std::string persistent = std::format("\tpersistent: {}\n", sc<bool>(r.isPersistent) ? boolToString(r.isPersistent) : "<unset>");
        const std::string gapsIn     = sc<bool>(r.gapsIn) ? std::format("\tgapsIn: {} {} {} {}\n", std::to_string(r.gapsIn.value().m_top), std::to_string(r.gapsIn.value().m_right),
                                                                        std::to_string(r.gapsIn.value().m_bottom), std::to_string(r.gapsIn.value().m_left)) :
                                                            std::format("\tgapsIn: <unset>\n");
        const std::string gapsOut    = sc<bool>(r.gapsOut) ?
               std::format("\tgapsOut: {} {} {} {}\n", std::to_string(r.gapsOut.value().m_top), std::to_string(r.gapsOut.value().m_right), std::to_string(r.gapsOut.value().m_bottom),
                           std::to_string(r.gapsOut.value().m_left)) :
               std::format("\tgapsOut: <unset>\n");
        const std::string borderSize = std::format("\tborderSize: {}\n", sc<bool>(r.borderSize) ? std::to_string(r.borderSize.value()) : "<unset>");
        const std::string border     = std::format("\tborder: {}\n", sc<bool>(r.noBorder) ? boolToString(!r.noBorder.value()) : "<unset>");
        const std::string rounding   = std::format("\trounding: {}\n", sc<bool>(r.noRounding) ? boolToString(!r.noRounding.value()) : "<unset>");
        const std::string decorate   = std::format("\tdecorate: {}\n", sc<bool>(r.decorate) ? boolToString(r.decorate.value()) : "<unset>");
        const std::string shadow     = std::format("\tshadow: {}\n", sc<bool>(r.noShadow) ? boolToString(!r.noShadow.value()) : "<unset>");
        const std::string defaultName = std::format("\tdefaultName: {}\n", r.defaultName.value_or("<unset>"));

        std::string       result = std::format("Workspace rule {}:\n{}{}{}{}{}{}{}{}{}{}{}\n", escapeJSONStrings(r.workspaceString), monitor, default_, persistent, gapsIn, gapsOut,
                                               borderSize, border, rounding, decorate, shadow, defaultName);

        return result;
    }
}

static std::string activeWorkspaceRequest(eHyprCtlOutputFormat format, std::string request) {
    if (!g_pCompositor->m_lastMonitor)
        return "unsafe state";

    std::string result = "";
    auto        w      = g_pCompositor->m_lastMonitor->m_activeWorkspace;

    if (!valid(w))
        return "internal error";

    return CHyprCtl::getWorkspaceData(w, format);
}

static std::string workspacesRequest(eHyprCtlOutputFormat format, std::string request) {
    std::string result = "";

    if (format == eHyprCtlOutputFormat::FORMAT_JSON) {
        result += "[";
        for (auto const& w : g_pCompositor->getWorkspaces()) {
            result += CHyprCtl::getWorkspaceData(w.lock(), format);
            result += ",";
        }

        trimTrailingComma(result);
        result += "]";
    } else {
        for (auto const& w : g_pCompositor->getWorkspaces()) {
            result += CHyprCtl::getWorkspaceData(w.lock(), format);
        }
    }

    return result;
}

static std::string workspaceRulesRequest(eHyprCtlOutputFormat format, std::string request) {
    std::string result = "";
    if (format == eHyprCtlOutputFormat::FORMAT_JSON) {
        result += "[";
        for (auto const& r : g_pConfigManager->getAllWorkspaceRules()) {
            result += getWorkspaceRuleData(r, format);
            result += ",";
        }

        trimTrailingComma(result);
        result += "]";
    } else {
        for (auto const& r : g_pConfigManager->getAllWorkspaceRules()) {
            result += getWorkspaceRuleData(r, format);
        }
    }

    return result;
}

static std::string activeWindowRequest(eHyprCtlOutputFormat format, std::string request) {
    const auto PWINDOW = g_pCompositor->m_lastWindow.lock();

    if (!validMapped(PWINDOW))
        return format == eHyprCtlOutputFormat::FORMAT_JSON ? "{}" : "Invalid";

    auto result = CHyprCtl::getWindowData(PWINDOW, format);

    if (format == eHyprCtlOutputFormat::FORMAT_JSON)
        result.pop_back();

    return result;
}

static std::string layersRequest(eHyprCtlOutputFormat format, std::string request) {
    std::string result = "";

    if (format == eHyprCtlOutputFormat::FORMAT_JSON) {
        result += "{\n";

        for (auto const& mon : g_pCompositor->m_monitors) {
            result += std::format(
                R"#("{}": {{
    "levels": {{
)#",
                escapeJSONStrings(mon->m_name));

            int layerLevel = 0;
            for (auto const& level : mon->m_layerSurfaceLayers) {
                result += std::format(
                    R"#(
        "{}": [
)#",
                    layerLevel);
                for (auto const& layer : level) {
                    result += std::format(
                        R"#(                {{
                    "address": "0x{:x}",
                    "x": {},
                    "y": {},
                    "w": {},
                    "h": {},
                    "namespace": "{}",
                    "pid": {}
                }},)#",
                        rc<uintptr_t>(layer.get()), layer->m_geometry.x, layer->m_geometry.y, layer->m_geometry.width, layer->m_geometry.height,
                        escapeJSONStrings(layer->m_namespace), layer->getPID());
                }

                trimTrailingComma(result);

                if (!level.empty())
                    result += "\n        ";

                result += "],";

                layerLevel++;
            }

            trimTrailingComma(result);

            result += "\n    }\n},";
        }

        trimTrailingComma(result);

        result += "\n}\n";

    } else {
        for (auto const& mon : g_pCompositor->m_monitors) {
            result += std::format("Monitor {}:\n", mon->m_name);
            int                                     layerLevel = 0;
            static const std::array<std::string, 4> levelNames = {"background", "bottom", "top", "overlay"};
            for (auto const& level : mon->m_layerSurfaceLayers) {
                result += std::format("\tLayer level {} ({}):\n", layerLevel, levelNames[layerLevel]);

                for (auto const& layer : level) {
                    result += std::format("\t\tLayer {:x}: xywh: {} {} {} {}, namespace: {}, pid: {}\n", rc<uintptr_t>(layer.get()), layer->m_geometry.x, layer->m_geometry.y,
                                          layer->m_geometry.width, layer->m_geometry.height, layer->m_namespace, layer->getPID());
                }

                layerLevel++;
            }
            result += "\n\n";
        }
    }

    return result;
}

static std::string layoutsRequest(eHyprCtlOutputFormat format, std::string request) {
    std::string result = "";
    if (format == eHyprCtlOutputFormat::FORMAT_JSON) {
        result += "[";

        for (auto const& m : g_pLayoutManager->getAllLayoutNames()) {
            result += std::format(
                R"#(
    "{}",)#",
                m);
        }
        trimTrailingComma(result);

        result += "\n]\n";
    } else {
        for (auto const& m : g_pLayoutManager->getAllLayoutNames()) {
            result += std::format("{}\n", m);
        }
    }
    return result;
}

static std::string configErrorsRequest(eHyprCtlOutputFormat format, std::string request) {
    std::string result     = "";
    std::string currErrors = g_pConfigManager->getErrors();
    CVarList    errLines(currErrors, 0, '\n');
    if (format == eHyprCtlOutputFormat::FORMAT_JSON) {
        result += "[";
        for (const auto& line : errLines) {
            result += std::format(
                R"#(
	"{}",)#",

                escapeJSONStrings(line));
        }
        trimTrailingComma(result);
        result += "\n]\n";
    } else {
        for (const auto& line : errLines) {
            result += std::format("{}\n", line);
        }
    }
    return result;
}

static std::string devicesRequest(eHyprCtlOutputFormat format, std::string request) {
    std::string result = "";

    auto        getModState = [](SP<IKeyboard> keyboard, const char* xkbModName) -> bool {
        auto IDX = xkb_keymap_mod_get_index(keyboard->m_xkbKeymap, xkbModName);

        if (IDX == XKB_MOD_INVALID)
            return false;

        return (keyboard->m_modifiersState.locked & (1 << IDX)) > 0;
    };

    if (format == eHyprCtlOutputFormat::FORMAT_JSON) {
        result += "{\n";
        result += "\"mice\": [\n";

        for (auto const& m : g_pInputManager->m_pointers) {
            result += std::format(
                R"#(    {{
        "address": "0x{:x}",
        "name": "{}",
        "defaultSpeed": {:.5f},
        "scrollFactor": {:.2f}
    }},)#",
                rc<uintptr_t>(m.get()), escapeJSONStrings(m->m_hlName),
                m->aq() && m->aq()->getLibinputHandle() ? libinput_device_config_accel_get_default_speed(m->aq()->getLibinputHandle()) : 0.f, m->m_scrollFactor.value_or(-1));
        }

        trimTrailingComma(result);
        result += "\n],\n";

        result += "\"keyboards\": [\n";
        for (auto const& k : g_pInputManager->m_keyboards) {
            const auto INDEX_OPT = k->getActiveLayoutIndex();
            const auto KI        = INDEX_OPT.has_value() ? std::to_string(INDEX_OPT.value()) : "none";
            const auto KM        = k->getActiveLayout();
            result += std::format(
                R"#(    {{
        "address": "0x{:x}",
        "name": "{}",
        "rules": "{}",
        "model": "{}",
        "layout": "{}",
        "variant": "{}",
        "options": "{}",
        "active_layout_index": {},
        "active_keymap": "{}",
        "capsLock": {},
        "numLock": {},
        "main": {}
    }},)#",
                rc<uintptr_t>(k.get()), escapeJSONStrings(k->m_hlName), escapeJSONStrings(k->m_currentRules.rules), escapeJSONStrings(k->m_currentRules.model),
                escapeJSONStrings(k->m_currentRules.layout), escapeJSONStrings(k->m_currentRules.variant), escapeJSONStrings(k->m_currentRules.options), KI, escapeJSONStrings(KM),
                (getModState(k, XKB_MOD_NAME_CAPS) ? "true" : "false"), (getModState(k, XKB_MOD_NAME_NUM) ? "true" : "false"), (k->m_active ? "true" : "false"));
        }

        trimTrailingComma(result);
        result += "\n],\n";

        result += "\"tablets\": [\n";

        for (auto const& d : g_pInputManager->m_tabletPads) {
            result += std::format(
                R"#(    {{
        "address": "0x{:x}",
        "type": "tabletPad",
        "belongsTo": {{
            "address": "0x{:x}",
            "name": "{}"
        }}
    }},)#",
                rc<uintptr_t>(d.get()), rc<uintptr_t>(d->m_parent.get()), escapeJSONStrings(d->m_parent ? d->m_parent->m_hlName : ""));
        }

        for (auto const& d : g_pInputManager->m_tablets) {
            result += std::format(
                R"#(    {{
        "address": "0x{:x}",
        "name": "{}"
    }},)#",
                rc<uintptr_t>(d.get()), escapeJSONStrings(d->m_hlName));
        }

        for (auto const& d : g_pInputManager->m_tabletTools) {
            result += std::format(
                R"#(    {{
        "address": "0x{:x}",
        "type": "tabletTool",
    }},)#",
                rc<uintptr_t>(d.get()));
        }

        trimTrailingComma(result);
        result += "\n],\n";

        result += "\"touch\": [\n";

        for (auto const& d : g_pInputManager->m_touches) {
            result += std::format(
                R"#(    {{
        "address": "0x{:x}",
        "name": "{}"
    }},)#",
                rc<uintptr_t>(d.get()), escapeJSONStrings(d->m_hlName));
        }

        trimTrailingComma(result);
        result += "\n],\n";

        result += "\"switches\": [\n";

        for (auto const& d : g_pInputManager->m_switches) {
            result += std::format(
                R"#(    {{
        "address": "0x{:x}",
        "name": "{}"
    }},)#",
                rc<uintptr_t>(&d), escapeJSONStrings(d.pDevice ? d.pDevice->getName() : ""));
        }

        trimTrailingComma(result);
        result += "\n]\n";

        result += "}\n";

    } else {
        result += "mice:\n";

        for (auto const& m : g_pInputManager->m_pointers) {
            result += std::format("\tMouse at {:x}:\n\t\t{}\n\t\t\tdefault speed: {:.5f}\n\t\t\tscroll factor: {:.2f}\n", rc<uintptr_t>(m.get()), m->m_hlName,
                                  (m->aq() && m->aq()->getLibinputHandle() ? libinput_device_config_accel_get_default_speed(m->aq()->getLibinputHandle()) : 0.f),
                                  m->m_scrollFactor.value_or(-1));
        }

        result += "\n\nKeyboards:\n";

        for (auto const& k : g_pInputManager->m_keyboards) {
            const auto INDEX_OPT = k->getActiveLayoutIndex();
            const auto KI        = INDEX_OPT.has_value() ? std::to_string(INDEX_OPT.value()) : "none";
            const auto KM        = k->getActiveLayout();
            result += std::format("\tKeyboard at {:x}:\n\t\t{}\n\t\t\trules: r \"{}\", m \"{}\", l \"{}\", v \"{}\", o \"{}\"\n\t\t\tactive layout index: {}\n\t\t\tactive keymap: "
                                  "{}\n\t\t\tcapsLock: "
                                  "{}\n\t\t\tnumLock: {}\n\t\t\tmain: {}\n",
                                  rc<uintptr_t>(k.get()), k->m_hlName, k->m_currentRules.rules, k->m_currentRules.model, k->m_currentRules.layout, k->m_currentRules.variant,
                                  k->m_currentRules.options, KI, KM, (getModState(k, XKB_MOD_NAME_CAPS) ? "yes" : "no"), (getModState(k, XKB_MOD_NAME_NUM) ? "yes" : "no"),
                                  (k->m_active ? "yes" : "no"));
        }

        result += "\n\nTablets:\n";

        for (auto const& d : g_pInputManager->m_tabletPads) {
            result +=
                std::format("\tTablet Pad at {:x} (belongs to {:x} -> {})\n", rc<uintptr_t>(d.get()), rc<uintptr_t>(d->m_parent.get()), d->m_parent ? d->m_parent->m_hlName : "");
        }

        for (auto const& d : g_pInputManager->m_tablets) {
            result += std::format("\tTablet at {:x}:\n\t\t{}\n\t\t\tsize: {}x{}mm\n", rc<uintptr_t>(d.get()), d->m_hlName, d->aq()->physicalSize.x, d->aq()->physicalSize.y);
        }

        for (auto const& d : g_pInputManager->m_tabletTools) {
            result += std::format("\tTablet Tool at {:x}\n", rc<uintptr_t>(d.get()));
        }

        result += "\n\nTouch:\n";

        for (auto const& d : g_pInputManager->m_touches) {
            result += std::format("\tTouch Device at {:x}:\n\t\t{}\n", rc<uintptr_t>(d.get()), d->m_hlName);
        }

        result += "\n\nSwitches:\n";

        for (auto const& d : g_pInputManager->m_switches) {
            result += std::format("\tSwitch Device at {:x}:\n\t\t{}\n", rc<uintptr_t>(&d), d.pDevice ? d.pDevice->getName() : "");
        }
    }

    return result;
}

static std::string animationsRequest(eHyprCtlOutputFormat format, std::string request) {
    std::string ret = "";
    if (format == eHyprCtlOutputFormat::FORMAT_NORMAL) {
        ret += "animations:\n";

        for (auto const& ac : g_pConfigManager->getAnimationConfig()) {
            ret += std::format("\n\tname: {}\n\t\toverriden: {}\n\t\tbezier: {}\n\t\tenabled: {}\n\t\tspeed: {:.2f}\n\t\tstyle: {}\n", ac.first, sc<int>(ac.second->overridden),
                               ac.second->internalBezier, ac.second->internalEnabled, ac.second->internalSpeed, ac.second->internalStyle);
        }

        ret += "beziers:\n";

        for (auto const& bz : g_pAnimationManager->getAllBeziers()) {
            auto& controlPoints = bz.second->getControlPoints();
            ret += std::format("\n\tname: {}\n\t\tX0: {:.2f}\n\t\tY0: {:.2f}\n\t\tX1: {:.2f}\n\t\tY1: {:.2f}", bz.first, controlPoints[1].x, controlPoints[1].y, controlPoints[2].x,
                               controlPoints[2].y);
        }
    } else {
        // json

        ret += "[[";
        for (auto const& ac : g_pConfigManager->getAnimationConfig()) {
            ret += std::format(R"#(
{{
    "name": "{}",
    "overridden": {},
    "bezier": "{}",
    "enabled": {},
    "speed": {:.2f},
    "style": "{}"
}},)#",
                               ac.first, ac.second->overridden ? "true" : "false", escapeJSONStrings(ac.second->internalBezier), ac.second->internalEnabled ? "true" : "false",
                               ac.second->internalSpeed, escapeJSONStrings(ac.second->internalStyle));
        }

        ret[ret.length() - 1] = ']';

        ret += ",\n[";

        for (auto const& bz : g_pAnimationManager->getAllBeziers()) {
            auto& controlPoints = bz.second->getControlPoints();
            ret += std::format(R"#(
{{
    "name": "{}",
    "X0": {:.2f},
    "Y0": {:.2f},
    "X1": {:.2f},
    "Y1": {:.2f}
}},)#",
                               escapeJSONStrings(bz.first), controlPoints[1].x, controlPoints[1].y, controlPoints[2].x, controlPoints[2].y);
        }

        trimTrailingComma(ret);

        ret += "]]";
    }

    return ret;
}

static std::string rollinglogRequest(eHyprCtlOutputFormat format, std::string request) {
    std::string result = "";

    if (format == eHyprCtlOutputFormat::FORMAT_JSON) {
        result += "[\n\"log\":\"";
        result += escapeJSONStrings(Debug::m_rollingLog);
        result += "\"]";
    } else {
        result = Debug::m_rollingLog;
    }

    return result;
}

static std::string globalShortcutsRequest(eHyprCtlOutputFormat format, std::string request) {
    std::string ret       = "";
    const auto  SHORTCUTS = PROTO::globalShortcuts->getAllShortcuts();
    if (format == eHyprCtlOutputFormat::FORMAT_NORMAL) {
        for (auto const& sh : SHORTCUTS) {
            ret += std::format("{}:{} -> {}\n", sh.appid, sh.id, sh.description);
        }
        if (ret.empty())
            ret = "none";
    } else {
        ret += "[";
        for (auto const& sh : SHORTCUTS) {
            ret += std::format(R"#(
{{
    "name": "{}",
    "description": "{}"
}},)#",
                               escapeJSONStrings(sh.appid + ":" + sh.id), escapeJSONStrings(sh.description));
        }
        trimTrailingComma(ret);
        ret += "]\n";
    }

    return ret;
}

static std::string bindsRequest(eHyprCtlOutputFormat format, std::string request) {
    std::string ret = "";
    if (format == eHyprCtlOutputFormat::FORMAT_NORMAL) {
        for (auto const& kb : g_pKeybindManager->m_keybinds) {
            ret += "bind";
            if (kb->locked)
                ret += "l";
            if (kb->mouse)
                ret += "m";
            if (kb->release)
                ret += "r";
            if (kb->repeat)
                ret += "e";
            if (kb->nonConsuming)
                ret += "n";
            if (kb->hasDescription)
                ret += "d";

            ret += std::format("\n\tmodmask: {}\n\tsubmap: {}\n\tkey: {}\n\tkeycode: {}\n\tcatchall: {}\n\tdescription: {}\n\tdispatcher: {}\n\targ: {}\n\n", kb->modmask,
                               kb->submap, kb->key, kb->keycode, kb->catchAll, kb->description, kb->handler, kb->arg);
        }
    } else {
        // json
        ret += "[";
        for (auto const& kb : g_pKeybindManager->m_keybinds) {
            ret += std::format(
                R"#(
{{
    "locked": {},
    "mouse": {},
    "release": {},
    "repeat": {},
    "longPress": {},
    "non_consuming": {},
    "has_description": {},
    "modmask": {},
    "submap": "{}",
    "key": "{}",
    "keycode": {},
    "catch_all": {},
    "description": "{}",
    "dispatcher": "{}",
    "arg": "{}"
}},)#",
                kb->locked ? "true" : "false", kb->mouse ? "true" : "false", kb->release ? "true" : "false", kb->repeat ? "true" : "false", kb->longPress ? "true" : "false",
                kb->nonConsuming ? "true" : "false", kb->hasDescription ? "true" : "false", kb->modmask, escapeJSONStrings(kb->submap), escapeJSONStrings(kb->key), kb->keycode,
                kb->catchAll ? "true" : "false", escapeJSONStrings(kb->description), escapeJSONStrings(kb->handler), escapeJSONStrings(kb->arg));
        }
        trimTrailingComma(ret);
        ret += "]";
    }

    return ret;
}

std::string versionRequest(eHyprCtlOutputFormat format, std::string request) {

    auto commitMsg = trim(GIT_COMMIT_MESSAGE);
    std::ranges::replace(commitMsg, '#', ' ');

    if (format == eHyprCtlOutputFormat::FORMAT_NORMAL) {
        std::string result = std::format("Hyprland {} built from branch {} at commit {} {} ({}).\n"
                                         "Date: {}\n"
                                         "Tag: {}, commits: {}\n",
                                         HYPRLAND_VERSION, GIT_BRANCH, GIT_COMMIT_HASH, GIT_DIRTY, commitMsg, GIT_COMMIT_DATE, GIT_TAG, GIT_COMMITS);

        result += "\n";
        result += getBuiltSystemLibraryNames();
        result += "\n";

#if (!ISDEBUG && !defined(NO_XWAYLAND))
        result += "no flags were set\n";
#else
        result += "flags set:\n";
#if ISDEBUG
        result += "debug\n";
#endif
#ifdef NO_XWAYLAND
        result += "no xwayland\n";
#endif
#endif
        return result;
    } else {
        std::string result = std::format(
            R"#({{
    "branch": "{}",
    "commit": "{}",
    "version": "{}",
    "dirty": {},
    "commit_message": "{}",
    "commit_date": "{}",
    "tag": "{}",
    "commits": "{}",
    "buildAquamarine": "{}",
    "buildHyprlang": "{}",
    "buildHyprutils": "{}",
    "buildHyprcursor": "{}",
    "buildHyprgraphics": "{}",
    "systemAquamarine": "{}",
    "systemHyprlang": "{}",
    "systemHyprutils": "{}",
    "systemHyprcursor": "{}",
    "systemHyprgraphics": "{}",
    "flags": [)#",
            GIT_BRANCH, GIT_COMMIT_HASH, HYPRLAND_VERSION, (strcmp(GIT_DIRTY, "dirty") == 0 ? "true" : "false"), escapeJSONStrings(commitMsg), GIT_COMMIT_DATE, GIT_TAG,
            GIT_COMMITS, AQUAMARINE_VERSION, HYPRLANG_VERSION, HYPRUTILS_VERSION, HYPRCURSOR_VERSION, HYPRGRAPHICS_VERSION, getSystemLibraryVersion("aquamarine"),
            getSystemLibraryVersion("hyprlang"), getSystemLibraryVersion("hyprutils"), getSystemLibraryVersion("hyprcursor"), getSystemLibraryVersion("hyprgraphics"));

#if ISDEBUG
        result += "\"debug\",";
#endif
#ifdef NO_XWAYLAND
        result += "\"no xwayland\",";
#endif

        trimTrailingComma(result);

        result += "]\n}";

        return result;
    }

    return ""; // make the compiler happy
}

std::string systemInfoRequest(eHyprCtlOutputFormat format, std::string request) {
    std::string result = versionRequest(eHyprCtlOutputFormat::FORMAT_NORMAL, "");

    static auto check   = [](bool y) -> std::string { return y ? "✔️" : "❌"; };
    static auto backend = [](Aquamarine::eBackendType t) -> std::string {
        switch (t) {
            case Aquamarine::AQ_BACKEND_DRM: return "drm";
            case Aquamarine::AQ_BACKEND_HEADLESS: return "headless";
            case Aquamarine::AQ_BACKEND_WAYLAND: return "wayland";
            default: break;
        }
        return "?";
    };

    result += "\n\nSystem Information:\n";

    struct utsname unameInfo;

    uname(&unameInfo);

    result += "System name: " + std::string{unameInfo.sysname} + "\n";
    result += "Node name: " + std::string{unameInfo.nodename} + "\n";
    result += "Release: " + std::string{unameInfo.release} + "\n";
    result += "Version: " + std::string{unameInfo.version} + "\n";
    result += "\n";
    result += getBuiltSystemLibraryNames();
    result += "\n";

    result += "\n\n";

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
    result += "GPU information: \n" + GPUINFO;
    if (GPUINFO.contains("NVIDIA") && std::filesystem::exists("/proc/driver/nvidia/version")) {
        std::ifstream file("/proc/driver/nvidia/version");
        std::string   line;
        if (file.is_open()) {
            while (std::getline(file, line)) {
                if (!line.contains("NVRM"))
                    continue;
                result += line;
                result += "\n";
            }
        } else
            result += "error";
    }
    result += "\n\n";

    if (std::ifstream file("/etc/os-release"); file.is_open()) {
        std::stringstream buffer;
        buffer << file.rdbuf();
        result += "os-release: " + buffer.str() + "\n\n";
    } else
        result += "os-release: error\n\n";

    result += "plugins:\n";
    if (g_pPluginSystem) {
        for (auto const& pl : g_pPluginSystem->getAllPlugins()) {
            result += std::format("  {} by {} ver {}\n", pl->m_name, pl->m_author, pl->m_version);
        }
    } else
        result += "\tunknown: not runtime\n";

    if (g_pHyprOpenGL) {
        result += std::format("\nExplicit sync: {}", g_pHyprOpenGL->m_exts.EGL_ANDROID_native_fence_sync_ext ? "supported" : "missing");
        result += std::format("\nGL ver: {}", g_pHyprOpenGL->m_eglContextVersion == CHyprOpenGLImpl::EGL_CONTEXT_GLES_3_2 ? "3.2" : "3.0");
    }

    if (g_pCompositor) {
        result += std::format("\nBackend: {}", g_pCompositor->m_aqBackend->hasSession() ? "drm" : "sessionless");

        result += "\n\nMonitor info:";

        for (const auto& m : g_pCompositor->m_monitors) {
            result += std::format("\n\tPanel {}: {}x{}, {} {} {} {} -> backend {}\n\t\texplicit {}\n\t\tedid:\n\t\t\thdr {}\n\t\t\tchroma {}\n\t\t\tbt2020 {}\n\t\tvrr capable "
                                  "{}\n\t\tnon-desktop {}\n\t\t",
                                  m->m_name, sc<int>(m->m_pixelSize.x), sc<int>(m->m_pixelSize.y), m->m_output->name, m->m_output->make, m->m_output->model, m->m_output->serial,
                                  backend(m->m_output->getBackend()->type()), check(m->m_output->supportsExplicit), check(m->m_output->parsedEDID.hdrMetadata.has_value()),
                                  check(m->m_output->parsedEDID.chromaticityCoords.has_value()), check(m->m_output->parsedEDID.supportsBT2020), check(m->m_output->vrrCapable),
                                  check(m->m_output->nonDesktop));
        }
    }

    if (g_pHyprCtl && g_pHyprCtl->m_currentRequestParams.sysInfoConfig) {
        result += "\n======Config-Start======\n";
        result += g_pConfigManager->getConfigString();
        result += "\n======Config-End========\n";
    }

    return result;
}

static std::string dispatchRequest(eHyprCtlOutputFormat format, std::string in) {
    // get rid of the dispatch keyword
    in = in.substr(in.find_first_of(' ') + 1);

    const auto DISPATCHSTR = in.substr(0, in.find_first_of(' '));

    auto       DISPATCHARG = std::string();
    if (sc<int>(in.find_first_of(' ')) != -1)
        DISPATCHARG = in.substr(in.find_first_of(' ') + 1);

    const auto DISPATCHER = g_pKeybindManager->m_dispatchers.find(DISPATCHSTR);
    if (DISPATCHER == g_pKeybindManager->m_dispatchers.end())
        return "Invalid dispatcher";

    SDispatchResult res = DISPATCHER->second(DISPATCHARG);

    Debug::log(LOG, "Hyprctl: dispatcher {} : {}{}", DISPATCHSTR, DISPATCHARG, res.success ? "" : " -> " + res.error);

    return res.success ? "ok" : res.error;
}

static std::string dispatchKeyword(eHyprCtlOutputFormat format, std::string in) {
    // Find the first space to strip the keyword keyword
    auto const firstSpacePos = in.find_first_of(' ');
    if (firstSpacePos == std::string::npos) // Handle the case where there's no space found (invalid input)
        return "Invalid input: no space found";

    // Strip the keyword
    in = in.substr(firstSpacePos + 1);

    // Find the next space for the COMMAND and VALUE
    auto const secondSpacePos = in.find_first_of(' ');
    if (secondSpacePos == std::string::npos) // Handle the case where there's no second space (invalid input)
        return "Invalid input: command and value not properly formatted";

    // Extract COMMAND and VALUE
    const auto COMMAND = in.substr(0, secondSpacePos);
    const auto VALUE   = in.substr(secondSpacePos + 1);

    // If COMMAND is empty, handle accordingly
    if (COMMAND.empty())
        return "Invalid input: command is empty";

    std::string retval = g_pConfigManager->parseKeyword(COMMAND, VALUE);

    // if we are executing a dynamic source we have to reload everything, so every if will have a check for source.
    if (COMMAND == "monitor" || COMMAND == "source")
        g_pConfigManager->m_wantsMonitorReload = true; // for monitor keywords

    if (COMMAND.contains("monitorv2"))
        g_pEventLoopManager->doLater([] { g_pConfigManager->m_wantsMonitorReload = true; });

    if (COMMAND.contains("input") || COMMAND.contains("device") || COMMAND == "source") {
        g_pInputManager->setKeyboardLayout();     // update kb layout
        g_pInputManager->setPointerConfigs();     // update mouse cfgs
        g_pInputManager->setTouchDeviceConfigs(); // update touch device cfgs
        g_pInputManager->setTabletConfigs();      // update tablets
    }

    static auto PLAYOUT = CConfigValue<std::string>("general:layout");

    if (COMMAND.contains("general:layout"))
        g_pLayoutManager->switchToLayout(*PLAYOUT); // update layout

    if (COMMAND.contains("decoration:screen_shader") || COMMAND == "source")
        g_pHyprOpenGL->m_reloadScreenShader = true;

    if (COMMAND.contains("blur") || COMMAND == "source") {
        for (auto& [m, rd] : g_pHyprOpenGL->m_monitorRenderResources) {
            rd.blurFBDirty = true;
        }
    }

    if (COMMAND.contains("misc:disable_autoreload"))
        g_pConfigManager->updateWatcher();

    // decorations will probably need a repaint
    if (COMMAND.contains("decoration:") || COMMAND.contains("border") || COMMAND == "workspace" || COMMAND.contains("zoom_factor") || COMMAND == "source" ||
        COMMAND.starts_with("windowrule")) {
        static auto PZOOMFACTOR = CConfigValue<Hyprlang::FLOAT>("cursor:zoom_factor");
        for (auto const& m : g_pCompositor->m_monitors) {
            *(m->m_cursorZoom) = *PZOOMFACTOR;
            g_pHyprRenderer->damageMonitor(m);
            g_pLayoutManager->getCurrentLayout()->recalculateMonitor(m->m_id);
        }
    }

    if (COMMAND.contains("workspace"))
        g_pConfigManager->ensurePersistentWorkspacesPresent();

    Debug::log(LOG, "Hyprctl: keyword {} : {}", COMMAND, VALUE);

    if (retval.empty())
        return "ok";

    return retval;
}

static std::string reloadRequest(eHyprCtlOutputFormat format, std::string request) {

    const auto REQMODE = request.substr(request.find_last_of(' ') + 1);

    if (REQMODE == "config-only")
        g_pConfigManager->m_noMonitorReload = true;

    g_pConfigManager->reload();

    return "ok";
}

static std::string killRequest(eHyprCtlOutputFormat format, std::string request) {
    g_pInputManager->setClickMode(CLICKMODE_KILL);

    return "ok";
}

static std::string splashRequest(eHyprCtlOutputFormat format, std::string request) {
    return g_pCompositor->m_currentSplash;
}

static std::string cursorPosRequest(eHyprCtlOutputFormat format, std::string request) {
    const auto CURSORPOS = g_pInputManager->getMouseCoordsInternal().floor();

    if (format == eHyprCtlOutputFormat::FORMAT_NORMAL) {
        return std::format("{}, {}", sc<int>(CURSORPOS.x), sc<int>(CURSORPOS.y));
    } else {
        return std::format(R"#(
{{
    "x": {},
    "y": {}
}}
)#",
                           sc<int>(CURSORPOS.x), sc<int>(CURSORPOS.y));
    }

    return "error";
}

static std::string dispatchBatch(eHyprCtlOutputFormat format, std::string request) {
    // split by ; ignores ; inside [] and adds ; on last command

    request                     = request.substr(9);
    std::string       reply     = "";
    const std::string DELIMITER = "\n\n\n";
    int               bracket   = 0;
    size_t            idx       = 0;

    for (size_t i = 0; i <= request.size(); ++i) {
        char ch = (i < request.size()) ? request[i] : ';';
        if (ch == '[')
            ++bracket;
        else if (ch == ']')
            --bracket;
        else if (ch == ';' && bracket == 0) {
            if (idx < i)
                reply += g_pHyprCtl->getReply(trim(request.substr(idx, i - idx))).append(DELIMITER);
            idx = i + 1;
            continue;
        }
    }

    return reply.substr(0, std::max(sc<int>(reply.size() - DELIMITER.size()), 0));
}

static std::string dispatchSetCursor(eHyprCtlOutputFormat format, std::string request) {
    CVarList    vars(request, 0, ' ');

    const auto  SIZESTR = vars[vars.size() - 1];
    std::string theme   = "";
    for (size_t i = 1; i < vars.size() - 1; ++i)
        theme += vars[i] + " ";
    if (!theme.empty())
        theme.pop_back();

    int size = 0;
    try {
        size = std::stoi(SIZESTR);
    } catch (...) { return "size not int"; }

    if (size <= 0)
        return "size not positive";

    if (!g_pCursorManager->changeTheme(theme, size))
        return "failed to set cursor";

    return "ok";
}

static std::string switchXKBLayoutRequest(eHyprCtlOutputFormat format, std::string request) {
    CVarList      vars(request, 0, ' ');

    const auto    KB  = vars[1];
    const auto    CMD = vars[2];

    SP<IKeyboard> pKeyboard;

    auto          updateKeyboard = [](const SP<IKeyboard> KEEB, const std::string& CMD) -> std::optional<std::string> {
        const auto         LAYOUTS      = xkb_keymap_num_layouts(KEEB->m_xkbKeymap);
        xkb_layout_index_t activeLayout = 0;
        while (activeLayout < LAYOUTS) {
            if (xkb_state_layout_index_is_active(KEEB->m_xkbState, activeLayout, XKB_STATE_LAYOUT_EFFECTIVE) == 1)
                break;

            activeLayout++;
        }

        if (CMD == "next")
            KEEB->updateModifiers(KEEB->m_modifiersState.depressed, KEEB->m_modifiersState.latched, KEEB->m_modifiersState.locked, activeLayout > LAYOUTS ? 0 : activeLayout + 1);
        else if (CMD == "prev")
            KEEB->updateModifiers(KEEB->m_modifiersState.depressed, KEEB->m_modifiersState.latched, KEEB->m_modifiersState.locked,
                                  activeLayout == 0 ? LAYOUTS - 1 : activeLayout - 1);
        else {
            int requestedLayout = 0;
            try {
                requestedLayout = std::stoi(CMD);
            } catch (std::exception& e) { return "invalid arg 2"; }

            if (requestedLayout < 0 || sc<uint64_t>(requestedLayout) > LAYOUTS - 1) {
                return "layout idx out of range of " + std::to_string(LAYOUTS);
            }

            KEEB->updateModifiers(KEEB->m_modifiersState.depressed, KEEB->m_modifiersState.latched, KEEB->m_modifiersState.locked, requestedLayout);
        }

        return std::nullopt;
    };

    if (KB == "main" || KB == "active" || KB == "current") {
        for (auto const& k : g_pInputManager->m_keyboards) {
            if (!k->m_active)
                continue;

            pKeyboard = k;
            break;
        }
    } else if (KB == "all") {
        std::string result = "";
        for (auto const& k : g_pInputManager->m_keyboards) {
            auto res = updateKeyboard(k, CMD);
            if (res.has_value())
                result += *res + "\n";
        }
        return result.empty() ? "ok" : result;
    } else {
        auto k = std::ranges::find_if(g_pInputManager->m_keyboards, [&](const auto& other) { return other->m_hlName == deviceNameToInternalString(KB); });

        if (k == g_pInputManager->m_keyboards.end())
            return "device not found";

        pKeyboard = *k;
    }

    if (!pKeyboard)
        return "no device";

    auto result = updateKeyboard(pKeyboard, CMD);

    if (result.has_value())
        return *result;

    return "ok";
}

static std::string dispatchSeterror(eHyprCtlOutputFormat format, std::string request) {
    CVarList    vars(request, 0, ' ');

    std::string errorMessage = "";

    if (vars.size() < 3) {
        g_pHyprError->destroy();

        if (vars.size() == 2 && !vars[1].contains("dis"))
            return "var 1 not color or disable";

        return "ok";
    }

    const CHyprColor COLOR = configStringToInt(vars[1]).value_or(0);

    for (size_t i = 2; i < vars.size(); ++i)
        errorMessage += vars[i] + ' ';

    if (errorMessage.empty()) {
        g_pHyprError->destroy();
    } else {
        errorMessage.pop_back(); // pop last space
        g_pHyprError->queueCreate(errorMessage, COLOR);
    }

    return "ok";
}

static std::string dispatchSetProp(eHyprCtlOutputFormat format, std::string request) {
    auto result = g_pKeybindManager->m_dispatchers["setprop"](request.substr(request.find_first_of(' ') + 1));
    return "DEPRECATED: use hyprctl dispatch setprop instead" + (result.success ? "" : "\n" + result.error);
}

static std::string dispatchGetProp(eHyprCtlOutputFormat format, std::string request) {
    CVarList vars(request, 0, ' ');

    if (vars.size() < 3)
        return "not enough args";

    const auto WINREGEX = vars[1];
    const auto PROP     = vars[2];

    const auto PWINDOW = g_pCompositor->getWindowByRegex(WINREGEX);

    if (!PWINDOW)
        return "window not found";

    const bool FORMNORM = format == FORMAT_NORMAL;

    auto       sizeToString = [&](bool max) -> std::string {
        auto sizeValue = PWINDOW->m_windowData.minSize.valueOr(Vector2D(MIN_WINDOW_SIZE, MIN_WINDOW_SIZE));
        if (max)
            sizeValue = PWINDOW->m_windowData.maxSize.valueOr(Vector2D(INFINITY, INFINITY));

        if (FORMNORM)
            return std::format("{} {}", sizeValue.x, sizeValue.y);
        else {
            std::string xSizeString = (sizeValue.x != INFINITY) ? std::to_string(sizeValue.x) : "null";
            std::string ySizeString = (sizeValue.y != INFINITY) ? std::to_string(sizeValue.y) : "null";
            return std::format(R"({{"{}": [{},{}]}})", PROP, xSizeString, ySizeString);
        }
    };

    auto alphaToString = [&](CWindowOverridableVar<SAlphaValue>& alpha, bool getAlpha) -> std::string {
        if (FORMNORM) {
            if (getAlpha)
                return std::format("{}", alpha.valueOrDefault().alpha);
            else
                return std::format("{}", alpha.valueOrDefault().overridden);
        } else {
            if (getAlpha)
                return std::format(R"({{"{}": {}}})", PROP, alpha.valueOrDefault().alpha);
            else
                return std::format(R"({{"{}": {}}})", PROP, alpha.valueOrDefault().overridden);
        }
    };

    auto borderColorToString = [&](bool active) -> std::string {
        static auto PACTIVECOL              = CConfigValue<Hyprlang::CUSTOMTYPE>("general:col.active_border");
        static auto PINACTIVECOL            = CConfigValue<Hyprlang::CUSTOMTYPE>("general:col.inactive_border");
        static auto PNOGROUPACTIVECOL       = CConfigValue<Hyprlang::CUSTOMTYPE>("general:col.nogroup_border_active");
        static auto PNOGROUPINACTIVECOL     = CConfigValue<Hyprlang::CUSTOMTYPE>("general:col.nogroup_border");
        static auto PGROUPACTIVECOL         = CConfigValue<Hyprlang::CUSTOMTYPE>("group:col.border_active");
        static auto PGROUPINACTIVECOL       = CConfigValue<Hyprlang::CUSTOMTYPE>("group:col.border_inactive");
        static auto PGROUPACTIVELOCKEDCOL   = CConfigValue<Hyprlang::CUSTOMTYPE>("group:col.border_locked_active");
        static auto PGROUPINACTIVELOCKEDCOL = CConfigValue<Hyprlang::CUSTOMTYPE>("group:col.border_locked_inactive");

        const bool  GROUPLOCKED = PWINDOW->m_groupData.pNextWindow.lock() ? PWINDOW->getGroupHead()->m_groupData.locked : false;

        if (active) {
            auto* const       ACTIVECOL            = (CGradientValueData*)(PACTIVECOL.ptr())->getData();
            auto* const       NOGROUPACTIVECOL     = (CGradientValueData*)(PNOGROUPACTIVECOL.ptr())->getData();
            auto* const       GROUPACTIVECOL       = (CGradientValueData*)(PGROUPACTIVECOL.ptr())->getData();
            auto* const       GROUPACTIVELOCKEDCOL = (CGradientValueData*)(PGROUPACTIVELOCKEDCOL.ptr())->getData();
            const auto* const ACTIVECOLOR =
                !PWINDOW->m_groupData.pNextWindow.lock() ? (!PWINDOW->m_groupData.deny ? ACTIVECOL : NOGROUPACTIVECOL) : (GROUPLOCKED ? GROUPACTIVELOCKEDCOL : GROUPACTIVECOL);

            std::string borderColorString = PWINDOW->m_windowData.activeBorderColor.valueOr(*ACTIVECOLOR).toString();
            if (FORMNORM)
                return borderColorString;
            else
                return std::format(R"({{"{}": "{}"}})", PROP, borderColorString);
        } else {
            auto* const       INACTIVECOL            = (CGradientValueData*)(PINACTIVECOL.ptr())->getData();
            auto* const       NOGROUPINACTIVECOL     = (CGradientValueData*)(PNOGROUPINACTIVECOL.ptr())->getData();
            auto* const       GROUPINACTIVECOL       = (CGradientValueData*)(PGROUPINACTIVECOL.ptr())->getData();
            auto* const       GROUPINACTIVELOCKEDCOL = (CGradientValueData*)(PGROUPINACTIVELOCKEDCOL.ptr())->getData();
            const auto* const INACTIVECOLOR          = !PWINDOW->m_groupData.pNextWindow.lock() ? (!PWINDOW->m_groupData.deny ? INACTIVECOL : NOGROUPINACTIVECOL) :
                                                                                                  (GROUPLOCKED ? GROUPINACTIVELOCKEDCOL : GROUPINACTIVECOL);

            std::string       borderColorString = PWINDOW->m_windowData.inactiveBorderColor.valueOr(*INACTIVECOLOR).toString();
            if (FORMNORM)
                return borderColorString;
            else
                return std::format(R"({{"{}": "{}"}})", PROP, borderColorString);
        }
    };

    auto windowPropToString = [&](auto& prop) -> std::string {
        if (FORMNORM)
            return std::format("{}", prop.valueOrDefault());
        else
            return std::format(R"({{"{}": {}}})", PROP, prop.valueOrDefault());
    };

    if (PROP == "animationstyle") {
        auto& animationStyle = PWINDOW->m_windowData.animationStyle;
        if (FORMNORM)
            return animationStyle.valueOr("(unset)");
        else
            return std::format(R"({{"{}": "{}"}})", PROP, animationStyle.valueOr(""));
    } else if (PROP == "maxsize")
        return sizeToString(true);
    else if (PROP == "minsize")
        return sizeToString(false);
    else if (PROP == "alpha")
        return alphaToString(PWINDOW->m_windowData.alpha, true);
    else if (PROP == "alphainactive")
        return alphaToString(PWINDOW->m_windowData.alphaInactive, true);
    else if (PROP == "alphafullscreen")
        return alphaToString(PWINDOW->m_windowData.alphaFullscreen, true);
    else if (PROP == "alphaoverride")
        return alphaToString(PWINDOW->m_windowData.alpha, false);
    else if (PROP == "alphainactiveoverride")
        return alphaToString(PWINDOW->m_windowData.alphaInactive, false);
    else if (PROP == "alphafullscreenoverride")
        return alphaToString(PWINDOW->m_windowData.alphaFullscreen, false);
    else if (PROP == "activebordercolor")
        return borderColorToString(true);
    else if (PROP == "inactivebordercolor")
        return borderColorToString(false);
    else if (auto search = NWindowProperties::boolWindowProperties.find(PROP); search != NWindowProperties::boolWindowProperties.end())
        return windowPropToString(*search->second(PWINDOW));
    else if (auto search = NWindowProperties::intWindowProperties.find(PROP); search != NWindowProperties::intWindowProperties.end())
        return windowPropToString(*search->second(PWINDOW));
    else if (auto search = NWindowProperties::floatWindowProperties.find(PROP); search != NWindowProperties::floatWindowProperties.end())
        return windowPropToString(*search->second(PWINDOW));

    return "prop not found";
}

static std::string dispatchGetOption(eHyprCtlOutputFormat format, std::string request) {
    std::string curitem = "";

    auto        nextItem = [&]() {
        auto idx = request.find_first_of(' ');

        if (idx != std::string::npos) {
            curitem = request.substr(0, idx);
            request = request.substr(idx + 1);
        } else {
            curitem = request;
            request = "";
        }

        curitem = trim(curitem);
    };

    nextItem();
    nextItem();

    const auto VAR = g_pConfigManager->getHyprlangConfigValuePtr(curitem);

    if (!VAR)
        return "no such option";

    const auto VAL  = VAR->getValue();
    const auto TYPE = std::type_index(VAL.type());

    if (format == FORMAT_NORMAL) {
        if (TYPE == typeid(Hyprlang::INT))
            return std::format("int: {}\nset: {}", std::any_cast<Hyprlang::INT>(VAL), VAR->m_bSetByUser);
        else if (TYPE == typeid(Hyprlang::FLOAT))
            return std::format("float: {:2f}\nset: {}", std::any_cast<Hyprlang::FLOAT>(VAL), VAR->m_bSetByUser);
        else if (TYPE == typeid(Hyprlang::VEC2))
            return std::format("vec2: [{}, {}]\nset: {}", std::any_cast<Hyprlang::VEC2>(VAL).x, std::any_cast<Hyprlang::VEC2>(VAL).y, VAR->m_bSetByUser);
        else if (TYPE == typeid(Hyprlang::STRING))
            return std::format("str: {}\nset: {}", std::any_cast<Hyprlang::STRING>(VAL), VAR->m_bSetByUser);
        else if (TYPE == typeid(void*))
            return std::format("custom type: {}\nset: {}", sc<ICustomConfigValueData*>(std::any_cast<void*>(VAL))->toString(), VAR->m_bSetByUser);
    } else {
        if (TYPE == typeid(Hyprlang::INT))
            return std::format(R"({{"option": "{}", "int": {}, "set": {} }})", curitem, std::any_cast<Hyprlang::INT>(VAL), VAR->m_bSetByUser);
        else if (TYPE == typeid(Hyprlang::FLOAT))
            return std::format(R"({{"option": "{}", "float": {:2f}, "set": {} }})", curitem, std::any_cast<Hyprlang::FLOAT>(VAL), VAR->m_bSetByUser);
        else if (TYPE == typeid(Hyprlang::VEC2))
            return std::format(R"({{"option": "{}", "vec2": [{},{}], "set": {} }})", curitem, std::any_cast<Hyprlang::VEC2>(VAL).x, std::any_cast<Hyprlang::VEC2>(VAL).y,
                               VAR->m_bSetByUser);
        else if (TYPE == typeid(Hyprlang::STRING))
            return std::format(R"({{"option": "{}", "str": "{}", "set": {} }})", curitem, escapeJSONStrings(std::any_cast<Hyprlang::STRING>(VAL)), VAR->m_bSetByUser);
        else if (TYPE == typeid(void*))
            return std::format(R"({{"option": "{}", "custom": "{}", "set": {} }})", curitem, sc<ICustomConfigValueData*>(std::any_cast<void*>(VAL))->toString(), VAR->m_bSetByUser);
    }

    return "invalid type (internal error)";
}

static std::string decorationRequest(eHyprCtlOutputFormat format, std::string request) {
    CVarList   vars(request, 0, ' ');
    const auto PWINDOW = g_pCompositor->getWindowByRegex(vars[1]);

    if (!PWINDOW)
        return "none";

    std::string result = "";
    if (format == eHyprCtlOutputFormat::FORMAT_JSON) {
        result += "[";
        for (auto const& wd : PWINDOW->m_windowDecorations) {
            result += "{\n\"decorationName\": \"" + wd->getDisplayName() + "\",\n\"priority\": " + std::to_string(wd->getPositioningInfo().priority) + "\n},";
        }

        trimTrailingComma(result);
        result += "]";
    } else {
        result = +"Decoration\tPriority\n";
        for (auto const& wd : PWINDOW->m_windowDecorations) {
            result += wd->getDisplayName() + "\t" + std::to_string(wd->getPositioningInfo().priority) + "\n";
        }
    }

    return result;
}

static std::string dispatchOutput(eHyprCtlOutputFormat format, std::string request) {
    CVarList vars(request, 0, ' ');

    if (vars.size() < 2)
        return "not enough args";

    const auto MODE = vars[1];

    bool       added = false;

    if (!vars[3].empty()) {
        for (auto const& m : g_pCompositor->m_realMonitors) {
            if (m->m_name == vars[3])
                return "Name already taken";
        }
    }

    if (MODE == "create" || MODE == "add") {
        if (g_pCompositor->getMonitorFromName(vars[3]))
            return "A real monitor already uses that name.";

        for (auto const& impl : g_pCompositor->m_aqBackend->getImplementations() | std::views::reverse) {
            auto type = impl->type();

            if (type == Aquamarine::AQ_BACKEND_HEADLESS && (vars[2] == "headless" || vars[2] == "auto")) {
                added = true;
                impl->createOutput(vars[3]);
                break;
            }

            if (type == Aquamarine::AQ_BACKEND_WAYLAND && (vars[2] == "wayland" || vars[2] == "auto")) {
                added = true;
                impl->createOutput(vars[3]);
                break;
            }
        }

        if (!added)
            return "no backend replied to the request";

    } else if (MODE == "destroy" || MODE == "remove") {
        const auto PMONITOR = g_pCompositor->getMonitorFromName(vars[2]);

        if (!PMONITOR)
            return "output not found";

        if (!PMONITOR->m_createdByUser)
            return "cannot remove a real display. Use the monitor keyword.";

        PMONITOR->m_output->destroy();
    }

    return "ok";
}

static std::string dispatchPlugin(eHyprCtlOutputFormat format, std::string request) {
    CVarList vars(request, 0, ' ');

    if (vars.size() < 2)
        return "not enough args";

    const auto OPERATION = vars[1];
    const auto PATH      = vars[2];

    if (OPERATION == "load") {
        if (vars.size() < 3)
            return "not enough args";

        g_pHyprCtl->m_currentRequestParams.pendingPromise = CPromise<std::string>::make([PATH](SP<CPromiseResolver<std::string>> resolver) {
            g_pPluginSystem->loadPlugin(PATH)->then([resolver, PATH](SP<CPromiseResult<CPlugin*>> result) {
                if (result->hasError()) {
                    resolver->reject(result->error());
                    return;
                }

                resolver->resolve("ok");
            });
        });

        return "ok";
    } else if (OPERATION == "unload") {
        if (vars.size() < 3)
            return "not enough args";

        const auto PLUGIN = g_pPluginSystem->getPluginByPath(PATH);

        if (!PLUGIN)
            return "plugin not loaded";

        g_pPluginSystem->unloadPlugin(PLUGIN);
    } else if (OPERATION == "list") {
        const auto  PLUGINS = g_pPluginSystem->getAllPlugins();
        std::string result  = "";

        if (format == eHyprCtlOutputFormat::FORMAT_JSON) {
            result += "[";

            if (PLUGINS.empty())
                return "[]";

            for (auto const& p : PLUGINS) {
                result += std::format(
                    R"#(
{{
    "name": "{}",
    "author": "{}",
    "handle": "{:x}",
    "version": "{}",
    "description": "{}"
}},)#",
                    escapeJSONStrings(p->m_name), escapeJSONStrings(p->m_author), rc<uintptr_t>(p->m_handle), escapeJSONStrings(p->m_version), escapeJSONStrings(p->m_description));
            }
            trimTrailingComma(result);
            result += "]";
        } else {
            if (PLUGINS.empty())
                return "no plugins loaded";

            for (auto const& p : PLUGINS) {
                result += std::format("\nPlugin {} by {}:\n\tHandle: {:x}\n\tVersion: {}\n\tDescription: {}\n", p->m_name, p->m_author, rc<uintptr_t>(p->m_handle), p->m_version,
                                      p->m_description);
            }
        }

        return result;
    } else {
        return "unknown opt";
    }

    return "ok";
}

static std::string dispatchNotify(eHyprCtlOutputFormat format, std::string request) {
    CVarList vars(request, 0, ' ');

    if (vars.size() < 5)
        return "not enough args";

    const auto ICON = vars[1];

    if (!isNumber(ICON))
        return "invalid arg 1";

    int icon = -1;
    try {
        icon = std::stoi(ICON);
    } catch (std::exception& e) { return "invalid arg 1"; }

    if (icon > ICON_NONE || icon < 0)
        icon = ICON_NONE;

    const auto TIME = vars[2];
    int        time = 0;
    try {
        time = std::stoi(TIME);
    } catch (std::exception& e) { return "invalid arg 2"; }

    const auto COLOR_RESULT = configStringToInt(vars[3]);
    if (!COLOR_RESULT)
        return "invalid arg 3";
    CHyprColor color = *COLOR_RESULT;

    size_t     msgidx   = 4;
    float      fontsize = 13.f;
    if (vars[msgidx].length() > 9 && vars[msgidx].compare(0, 9, "fontsize:") == 0) {
        const auto FONTSIZE = vars[msgidx].substr(9);

        if (!isNumber(FONTSIZE, true))
            return "invalid fontsize kwarg";

        try {
            fontsize = std::stoi(FONTSIZE);
        } catch (std::exception& e) { return "invalid fontsize karg"; }

        ++msgidx;
    }

    if (vars.size() <= msgidx)
        return "not enough args";

    const auto MESSAGE = vars.join(" ", msgidx);

    g_pHyprNotificationOverlay->addNotification(MESSAGE, color, time, sc<eIcons>(icon), fontsize);

    return "ok";
}

static std::string dispatchDismissNotify(eHyprCtlOutputFormat format, std::string request) {
    CVarList vars(request, 0, ' ');

    int      amount = -1;

    if (vars.size() > 1) {
        const auto AMOUNT = vars[1];
        if (!isNumber(AMOUNT))
            return "invalid arg 1";

        try {
            amount = std::stoi(AMOUNT);
        } catch (std::exception& e) { return "invalid arg 1"; }
    }

    g_pHyprNotificationOverlay->dismissNotifications(amount);

    return "ok";
}

static std::string getIsLocked(eHyprCtlOutputFormat format, std::string request) {
    std::string lockedStr = g_pSessionLockManager->isSessionLocked() ? "true" : "false";
    if (format == eHyprCtlOutputFormat::FORMAT_JSON)
        lockedStr = std::format(R"#(
{{
    "locked": {}
}}
)#",
                                lockedStr);
    return lockedStr;
}

static std::string getDescriptions(eHyprCtlOutputFormat format, std::string request) {
    std::string json  = "[";
    const auto& DESCS = g_pConfigManager->getAllDescriptions();

    for (const auto& d : DESCS) {
        json += d.jsonify() + ",\n";
    }

    json.pop_back();
    json.pop_back();

    json += "]\n";
    return json;
}

static std::string submapRequest(eHyprCtlOutputFormat format, std::string request) {
    std::string submap = g_pKeybindManager->getCurrentSubmap();
    if (submap.empty())
        submap = "default";

    return format == FORMAT_JSON ? std::format("{{\"{}\"}}\n", escapeJSONStrings(submap)) : (submap + "\n");
}

static std::string reloadShaders(eHyprCtlOutputFormat format, std::string request) {
    if (g_pHyprOpenGL->initShaders())
        return format == FORMAT_JSON ? "{\"ok\": true}" : "ok";
    else
        return format == FORMAT_JSON ? "{\"ok\": false}" : "error";
}

CHyprCtl::CHyprCtl() {
    registerCommand(SHyprCtlCommand{"workspaces", true, workspacesRequest});
    registerCommand(SHyprCtlCommand{"workspacerules", true, workspaceRulesRequest});
    registerCommand(SHyprCtlCommand{"activeworkspace", true, activeWorkspaceRequest});
    registerCommand(SHyprCtlCommand{"clients", true, clientsRequest});
    registerCommand(SHyprCtlCommand{"kill", true, killRequest});
    registerCommand(SHyprCtlCommand{"activewindow", true, activeWindowRequest});
    registerCommand(SHyprCtlCommand{"layers", true, layersRequest});
    registerCommand(SHyprCtlCommand{"version", true, versionRequest});
    registerCommand(SHyprCtlCommand{"devices", true, devicesRequest});
    registerCommand(SHyprCtlCommand{"splash", true, splashRequest});
    registerCommand(SHyprCtlCommand{"cursorpos", true, cursorPosRequest});
    registerCommand(SHyprCtlCommand{"binds", true, bindsRequest});
    registerCommand(SHyprCtlCommand{"globalshortcuts", true, globalShortcutsRequest});
    registerCommand(SHyprCtlCommand{"systeminfo", true, systemInfoRequest});
    registerCommand(SHyprCtlCommand{"animations", true, animationsRequest});
    registerCommand(SHyprCtlCommand{"rollinglog", true, rollinglogRequest});
    registerCommand(SHyprCtlCommand{"layouts", true, layoutsRequest});
    registerCommand(SHyprCtlCommand{"configerrors", true, configErrorsRequest});
    registerCommand(SHyprCtlCommand{"locked", true, getIsLocked});
    registerCommand(SHyprCtlCommand{"descriptions", true, getDescriptions});
    registerCommand(SHyprCtlCommand{"submap", true, submapRequest});
    registerCommand(SHyprCtlCommand{.name = "reloadshaders", .exact = true, .fn = reloadShaders});

    registerCommand(SHyprCtlCommand{"monitors", false, monitorsRequest});
    registerCommand(SHyprCtlCommand{"reload", false, reloadRequest});
    registerCommand(SHyprCtlCommand{"plugin", false, dispatchPlugin});
    registerCommand(SHyprCtlCommand{"notify", false, dispatchNotify});
    registerCommand(SHyprCtlCommand{"dismissnotify", false, dispatchDismissNotify});
    registerCommand(SHyprCtlCommand{"setprop", false, dispatchSetProp});
    registerCommand(SHyprCtlCommand{"getprop", false, dispatchGetProp});
    registerCommand(SHyprCtlCommand{"seterror", false, dispatchSeterror});
    registerCommand(SHyprCtlCommand{"switchxkblayout", false, switchXKBLayoutRequest});
    registerCommand(SHyprCtlCommand{"output", false, dispatchOutput});
    registerCommand(SHyprCtlCommand{"dispatch", false, dispatchRequest});
    registerCommand(SHyprCtlCommand{"keyword", false, dispatchKeyword});
    registerCommand(SHyprCtlCommand{"setcursor", false, dispatchSetCursor});
    registerCommand(SHyprCtlCommand{"getoption", false, dispatchGetOption});
    registerCommand(SHyprCtlCommand{"decorations", false, decorationRequest});
    registerCommand(SHyprCtlCommand{"[[BATCH]]", false, dispatchBatch});

    startHyprCtlSocket();
}

CHyprCtl::~CHyprCtl() {
    if (m_eventSource)
        wl_event_source_remove(m_eventSource);
    if (!m_socketPath.empty())
        unlink(m_socketPath.c_str());
}

SP<SHyprCtlCommand> CHyprCtl::registerCommand(SHyprCtlCommand cmd) {
    return m_commands.emplace_back(makeShared<SHyprCtlCommand>(cmd));
}

void CHyprCtl::unregisterCommand(const SP<SHyprCtlCommand>& cmd) {
    std::erase(m_commands, cmd);
}

std::string CHyprCtl::getReply(std::string request) {
    auto format                          = eHyprCtlOutputFormat::FORMAT_NORMAL;
    bool reloadAll                       = false;
    m_currentRequestParams.all           = false;
    m_currentRequestParams.sysInfoConfig = false;

    // process flags for non-batch requests
    if (!request.starts_with("[[BATCH]]") && request.contains("/")) {
        long unsigned int sepIndex = 0;
        for (const auto& c : request) {
            if (c == '/') { // stop at separator
                break;
            }

            // after whitespace assume the first word as a keyword,
            // so its value can have slashes (e.g., a path)
            if (c == ' ') {
                sepIndex = request.size();
                break;
            }

            sepIndex++;

            if (c == 'j')
                format = eHyprCtlOutputFormat::FORMAT_JSON;
            else if (c == 'r')
                reloadAll = true;
            else if (c == 'a')
                m_currentRequestParams.all = true;
            else if (c == 'c')
                m_currentRequestParams.sysInfoConfig = true;
        }

        if (sepIndex < request.size())
            request = request.substr(sepIndex + 1); // remove flags and separator so we can compare the rest of the string
    }

    std::string result = "";

    // parse exact cmds first, then non-exact.
    for (auto const& cmd : m_commands) {
        if (!cmd->exact)
            continue;

        if (cmd->name == request) {
            result = cmd->fn(format, request);
            break;
        }
    }

    if (result.empty())
        for (auto const& cmd : m_commands) {
            if (cmd->exact)
                continue;

            if (request.starts_with(cmd->name)) {
                result = cmd->fn(format, request);
                break;
            }
        }

    if (result.empty())
        return "unknown request";

    if (reloadAll) {
        g_pConfigManager->m_wantsMonitorReload = true; // for monitor keywords

        g_pInputManager->setKeyboardLayout();     // update kb layout
        g_pInputManager->setPointerConfigs();     // update mouse cfgs
        g_pInputManager->setTouchDeviceConfigs(); // update touch device cfgs
        g_pInputManager->setTabletConfigs();      // update tablets

        static auto PLAYOUT = CConfigValue<std::string>("general:layout");

        g_pLayoutManager->switchToLayout(*PLAYOUT); // update layout

        g_pHyprOpenGL->m_reloadScreenShader = true;

        for (auto& [m, rd] : g_pHyprOpenGL->m_monitorRenderResources) {
            rd.blurFBDirty = true;
        }

        for (auto const& w : g_pCompositor->m_windows) {
            if (!w->m_isMapped || !w->m_workspace || !w->m_workspace->isVisible())
                continue;

            w->updateDynamicRules();
            g_pCompositor->updateWindowAnimatedDecorationValues(w);
        }

        for (auto const& m : g_pCompositor->m_monitors) {
            g_pHyprRenderer->damageMonitor(m);
            g_pLayoutManager->getCurrentLayout()->recalculateMonitor(m->m_id);
        }
    }

    return result;
}

std::string CHyprCtl::makeDynamicCall(const std::string& input) {
    return getReply(input);
}

static bool successWrite(int fd, const std::string& data, bool needLog = true) {
    if (write(fd, data.c_str(), data.length()) > 0)
        return true;

    if (errno == EAGAIN)
        return true;

    if (needLog)
        Debug::log(ERR, "Couldn't write to socket. Error: " + std::string(strerror(errno)));

    return false;
}

static void runWritingDebugLogThread(const int conn) {
    using namespace std::chrono_literals;
    Debug::log(LOG, "In followlog thread, got connection, start writing: {}", conn);
    //will be finished, when reading side close connection
    std::thread([conn]() {
        while (Debug::SRollingLogFollow::get().isRunning()) {
            if (Debug::SRollingLogFollow::get().isEmpty(conn)) {
                std::this_thread::sleep_for(1000ms);
                continue;
            }

            auto line = Debug::SRollingLogFollow::get().getLog(conn);
            if (!successWrite(conn, line))
                // We cannot write, when connection is closed. So thread will successfully exit by itself
                break;

            std::this_thread::sleep_for(100ms);
        }
        close(conn);
        Debug::SRollingLogFollow::get().stopFor(conn);
    }).detach();
}

static bool isFollowUpRollingLogRequest(const std::string& request) {
    return request.contains("rollinglog") && request.contains("f");
}

static int hyprCtlFDTick(int fd, uint32_t mask, void* data) {
    if (mask & WL_EVENT_ERROR || mask & WL_EVENT_HANGUP)
        return 0;

    if (!g_pHyprCtl->m_socketFD.isValid())
        return 0;

    sockaddr_in            clientAddress;
    socklen_t              clientSize = sizeof(clientAddress);

    const auto             ACCEPTEDCONNECTION = accept4(g_pHyprCtl->m_socketFD.get(), rc<sockaddr*>(&clientAddress), &clientSize, SOCK_CLOEXEC);

    std::array<char, 1024> readBuffer;

    // try to get creds
    CRED_T   creds;
    uint32_t len = sizeof(creds);
    if (getsockopt(ACCEPTEDCONNECTION, CRED_LVL, CRED_OPT, &creds, &len) == -1)
        Debug::log(ERR, "Hyprctl: failed to get peer creds");
    else {
        g_pHyprCtl->m_currentRequestParams.pid = creds.CRED_PID;
        Debug::log(LOG, "Hyprctl: new connection from pid {}", creds.CRED_PID);
    }

    //
    pollfd pollfds[1] = {
        {
            .fd     = ACCEPTEDCONNECTION,
            .events = POLLIN,
        },
    };

    int ret = poll(pollfds, 1, 5000);

    if (ret <= 0) {
        close(ACCEPTEDCONNECTION);
        return 0;
    }

    std::string request;
    while (true) {
        readBuffer.fill(0);
        auto messageSize = read(ACCEPTEDCONNECTION, readBuffer.data(), 1023);
        if (messageSize < 1)
            break;
        std::string recvd = readBuffer.data();
        request += recvd;
        if (messageSize < 1023)
            break;
    }

    std::string reply = "";

    try {
        reply = g_pHyprCtl->getReply(request);
    } catch (std::exception& e) {
        Debug::log(ERR, "Error in request: {}", e.what());
        reply = "Err: " + std::string(e.what());
    }

    if (g_pHyprCtl->m_currentRequestParams.pendingPromise) {
        // we have a promise pending
        g_pHyprCtl->m_currentRequestParams.pendingPromise->then([ACCEPTEDCONNECTION, request](SP<CPromiseResult<std::string>> result) {
            const auto RES = result->hasError() ? result->error() : result->result();
            successWrite(ACCEPTEDCONNECTION, RES);

            // No rollinglog or ensureMonitor here. These are only for plugins for now.

            close(ACCEPTEDCONNECTION);
        });

        g_pHyprCtl->m_currentRequestParams.pendingPromise.reset();
    } else {
        successWrite(ACCEPTEDCONNECTION, reply);

        if (isFollowUpRollingLogRequest(request)) {
            Debug::log(LOG, "Followup rollinglog request received. Starting thread to write to socket.");
            Debug::SRollingLogFollow::get().startFor(ACCEPTEDCONNECTION);
            runWritingDebugLogThread(ACCEPTEDCONNECTION);
            Debug::log(LOG, Debug::SRollingLogFollow::get().debugInfo());
        } else
            close(ACCEPTEDCONNECTION);

        if (g_pConfigManager->m_wantsMonitorReload)
            g_pConfigManager->ensureMonitorStatus();

        g_pHyprCtl->m_currentRequestParams.pid = 0;
    }

    return 0;
}

void CHyprCtl::startHyprCtlSocket() {
    m_socketFD = CFileDescriptor{socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0)};

    if (!m_socketFD.isValid()) {
        Debug::log(ERR, "Couldn't start the Hyprland Socket. (1) IPC will not work.");
        return;
    }

    sockaddr_un SERVERADDRESS = {.sun_family = AF_UNIX};

    m_socketPath = g_pCompositor->m_instancePath + "/.socket.sock";

    snprintf(SERVERADDRESS.sun_path, sizeof(SERVERADDRESS.sun_path), "%s", m_socketPath.c_str());

    if (bind(m_socketFD.get(), rc<sockaddr*>(&SERVERADDRESS), SUN_LEN(&SERVERADDRESS)) < 0) {
        Debug::log(ERR, "Couldn't start the Hyprland Socket. (2) IPC will not work.");
        return;
    }

    // 10 max queued.
    listen(m_socketFD.get(), 10);

    Debug::log(LOG, "Hypr socket started at {}", m_socketPath);

    m_eventSource = wl_event_loop_add_fd(g_pCompositor->m_wlEventLoop, m_socketFD.get(), WL_EVENT_READABLE, hyprCtlFDTick, nullptr);
}
