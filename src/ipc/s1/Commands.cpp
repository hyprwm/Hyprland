#include "Commands.hpp"
#include "../../desktop/view/window/WindowFullscreenPolicy.hpp"
#include "../../desktop/view/window/WindowGroupMembership.hpp"
#include "../../desktop/view/window/WindowPresentation.hpp"
#include "../../desktop/view/window/WindowSwallowController.hpp"
#include "../../output/Monitor.hpp"
#include "../../pointer/PointerManager.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <fstream>
#include <iterator>
#include <cstdio>
#include <cstdlib>
#include <sys/utsname.h>
#include <filesystem>
#include <ranges>
#include <sys/eventfd.h>

#include <sstream>
#include <string>
#include <typeindex>
#include <numeric>

#include <hyprutils/string/String.hpp>
#include <hyprutils/string/VarList.hpp>
using namespace Hyprutils::String;
#include <aquamarine/input/Input.hpp>

#include "../../config/shared/complex/ComplexDataTypes.hpp"
#include "../../config/lua/ConfigManager.hpp"
#include "../../config/ConfigValue.hpp"
#include "../../config/shared/parserUtils/ParserUtils.hpp"
#include "../../config/shared/inotify/ConfigWatcher.hpp"
#include "../../config/shared/workspace/WorkspaceRuleManager.hpp"
#include "../../config/shared/monitor/MonitorRuleManager.hpp"
#include "../../config/shared/animation/AnimationTree.hpp"
#include "../../config/supplementary/jeremy/Jeremy.hpp"
#include "../../config/values/ConfigValues.hpp"
#include "../../pointer/cursor/CursorManager.hpp"
#include "../../errorOverlay/Overlay.hpp"
#include "../../devices/IPointer.hpp"
#include "../../devices/IKeyboard.hpp"
#include "../../devices/ITouch.hpp"
#include "../../devices/Tablet.hpp"
#include "../../protocols/GlobalShortcuts.hpp"
#include "../../debug/log/RollingLogFollow.hpp"
#include "../../config/ConfigManager.hpp"
#include "../../helpers/MiscFunctions.hpp"
#include "../../keybinds/Manager.hpp"
#include "../../helpers/SystemInfo.hpp"
#include "../../helpers/string/StringUtils.hpp"
#include "../../desktop/view/LayerSurface.hpp"
#include "../../desktop/view/Group.hpp"
#include "../../desktop/rule/Engine.hpp"
#include "../../desktop/history/WindowHistoryTracker.hpp"
#include "../../desktop/state/FocusState.hpp"
#include "../../state/MonitorState.hpp"
#include "../../state/WorkspacePlacementController.hpp"
#include "../../state/WorkspaceState.hpp"
#include "../../version.h"

#include "../../Compositor.hpp"
#include "../../managers/input/InputManager.hpp"
#include "../../managers/XWaylandManager.hpp"
#include "../../managers/fullscreen/FullscreenController.hpp"
#include "../../plugins/PluginSystem.hpp"
#include "../../animation/AnimationManager.hpp"
#include "../../notification/NotificationOverlay.hpp"
#include "../../render/Renderer.hpp"
#include "../../render/OpenGL.hpp"
#include "../../layout/space/Space.hpp"
#include "../../layout/algorithm/Algorithm.hpp"
#include "../../layout/algorithm/TiledAlgorithm.hpp"
#include "../../layout/supplementary/WorkspaceAlgoMatcher.hpp"

using namespace Render::GL;
using namespace IPC::Socket1;
using eHyprCtlOutputFormat = eOutputFormat;

class CCommandFormatter {
  public:
    static std::string getWindowData(PHLWINDOW window, eOutputFormat format);
    static std::string getWorkspaceData(PHLWORKSPACE workspace, eOutputFormat format);
    static std::string getSolitaryBlockedReason(PHLMONITOR monitor, eOutputFormat format);
    static std::string getDSBlockedReason(PHLMONITOR monitor, eOutputFormat format);
    static std::string getTearingBlockedReason(PHLMONITOR monitor, eOutputFormat format);
    static std::string getMonitorData(PHLMONITOR monitor, eOutputFormat format);
};

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

const std::array<const char*, Monitor::CMonitor::SC_CHECKS_COUNT> SOLITARY_REASONS_JSON = {
    "\"UNKNOWN\"",   "\"NOTIFICATION\"", "\"LOCK\"",      "\"WORKSPACE\"", "\"WINDOWED\"", "\"DND\"",        "\"SPECIAL\"",  "\"ALPHA\"",       "\"OFFSET\"",
    "\"CANDIDATE\"", "\"OPAQUE\"",       "\"TRANSFORM\"", "\"OVERLAYS\"",  "\"FLOAT\"",    "\"WORKSPACES\"", "\"SURFACES\"", "\"CONFIGERROR\"", "\"FADEOUT\"",
};

const std::array<const char*, Monitor::CMonitor::SC_CHECKS_COUNT> SOLITARY_REASONS_TEXT = {
    "unknown reason",    "notification",     "session lock",     "invalid workspace", "windowed mode", "dnd active",
    "special workspace", "alpha channel",    "workspace offset", "missing candidate", "not opaque",    "surface transformations",
    "other overlays",    "floating windows", "other workspaces", "subsurfaces",       "config error",  "fadeout in progress",
};

std::string CCommandFormatter::getSolitaryBlockedReason(PHLMONITOR m, eHyprCtlOutputFormat format) {
    const auto reasons = m->isSolitaryBlocked(true);
    if (!reasons)
        return "null";

    std::string reasonStr = "";
    const auto  TEXTS     = format == eHyprCtlOutputFormat::FORMAT_JSON ? SOLITARY_REASONS_JSON : SOLITARY_REASONS_TEXT;

    for (uint32_t i = 0; i < Monitor::CMonitor::SC_CHECKS_COUNT; i++) {
        if (reasons & (1 << i)) {
            if (reasonStr != "")
                reasonStr += ",";
            reasonStr += TEXTS[i];
        }
    }

    return format == eHyprCtlOutputFormat::FORMAT_JSON ? std::format("[{}]", reasonStr) : reasonStr;
}

const std::array<const char*, Monitor::CMonitor::DS_CHECKS_COUNT> DS_REASONS_JSON = {
    "\"UNKNOWN\"",   "\"USER\"",    "\"WINDOWED\"",  "\"CONTENT\"", "\"MIRROR\"", "\"RECORD\"", "\"SW\"",
    "\"CANDIDATE\"", "\"SURFACE\"", "\"TRANSFORM\"", "\"DMA\"",     "\"FAILED\"", "\"CM\"",
};

const std::array<const char*, Monitor::CMonitor::DS_CHECKS_COUNT> DS_REASONS_TEXT = {
    "unknown reason",    "user settings",   "windowed mode",           "content type",   "monitor mirrors",   "screen record/screenshot", "software renders/cursors",
    "missing candidate", "invalid surface", "surface transformations", "invalid buffer", "activation failed", "color management",
};

std::string CCommandFormatter::getDSBlockedReason(PHLMONITOR m, eHyprCtlOutputFormat format) {
    const auto reasons = m->isDSBlocked(true);
    if (!reasons)
        return "null";

    std::string reasonStr = "";
    const auto  TEXTS     = format == eHyprCtlOutputFormat::FORMAT_JSON ? DS_REASONS_JSON : DS_REASONS_TEXT;

    for (int i = 0; i < Monitor::CMonitor::DS_CHECKS_COUNT; i++) {
        if (reasons & (1 << i)) {
            if (reasonStr != "")
                reasonStr += ",";
            reasonStr += TEXTS[i];
        }
    }

    return format == eHyprCtlOutputFormat::FORMAT_JSON ? std::format("[{}]", reasonStr) : reasonStr;
}

const std::array<const char*, Monitor::CMonitor::TC_CHECKS_COUNT> TEARING_REASONS_JSON = {
    "\"UNKNOWN\"", "\"NOT_TORN\"", "\"USER\"", "\"ZOOM\"", "\"SUPPORT\"", "\"CANDIDATE\"", "\"WINDOW\"", "\"HW_CURSOR\"",
};

const std::array<const char*, Monitor::CMonitor::TC_CHECKS_COUNT> TEARING_REASONS_TEXT = {"unknown reason",           "next frame is not torn", "user settings",   "zoom",
                                                                                          "not supported by monitor", "missing candidate",      "window settings", "hw cursor"};

std::string                                                       CCommandFormatter::getTearingBlockedReason(PHLMONITOR m, eHyprCtlOutputFormat format) {
    const auto reasons = m->isTearingBlocked(true);
    if (!reasons || (reasons == Monitor::CMonitor::TC_NOT_TORN && m->m_tearingState.activelyTearing))
        return "null";

    std::string reasonStr = "";
    const auto  TEXTS     = format == eHyprCtlOutputFormat::FORMAT_JSON ? TEARING_REASONS_JSON : TEARING_REASONS_TEXT;

    for (int i = 0; i < Monitor::CMonitor::TC_CHECKS_COUNT; i++) {
        if (reasons & (1 << i)) {
            if (reasonStr != "")
                reasonStr += ",";
            reasonStr += TEXTS[i];
        }
    }

    return format == eHyprCtlOutputFormat::FORMAT_JSON ? std::format("[{}]", reasonStr) : reasonStr;
}

std::string CCommandFormatter::getMonitorData(PHLMONITOR m, eHyprCtlOutputFormat format) {
    std::string result;
    if (!m->m_output || m->m_id == -1)
        return "";

    static auto tf = [](bool t) -> const char* { return t ? "true" : "false"; };
    static auto yn = [](bool t) -> const char* { return t ? "yes" : "no"; };

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
    "scale": {},
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
    "availableModes": [{}],
    "colorManagementPreset": "{}",
    "sdrBrightness": {},
    "sdrSaturation": {},
    "sdrMinLuminance": {},
    "sdrMaxLuminance": {},
    "hardwareCursorsInUse": {},
    "hardwareDetails": {{
        "backend": "{}",
        "hdr": {},
        "chroma": {},
        "bt2020": {},
        "vrrCapable": {}
    }}
}},)#",

            m->m_id, escapeJSONStrings(m->m_name), escapeJSONStrings(m->m_shortDescription), escapeJSONStrings(m->m_output->make), escapeJSONStrings(m->m_output->model),
            escapeJSONStrings(m->m_output->serial), sc<int>(m->m_pixelSize.x), sc<int>(m->m_pixelSize.y), sc<int>(m->m_output->physicalSize.x),
            sc<int>(m->m_output->physicalSize.y), m->m_refreshRate, sc<int>(m->m_position.x), sc<int>(m->m_position.y), m->activeWorkspaceID(),
            (!m->m_activeWorkspace ? "" : escapeJSONStrings(m->m_activeWorkspace->m_name)), m->activeSpecialWorkspaceID(),
            escapeJSONStrings(m->m_activeSpecialWorkspace ? m->m_activeSpecialWorkspace->m_name : ""), sc<int>(m->m_reservedArea.left()), sc<int>(m->m_reservedArea.top()),
            sc<int>(m->m_reservedArea.right()), sc<int>(m->m_reservedArea.bottom()), m->m_scale, sc<int>(m->m_transform), tf(m == Desktop::focusState()->monitor()),
            tf(m->m_dpmsStatus), tf(m->m_output->state->state().adaptiveSync), rc<uint64_t>(m->m_solitaryClient.get()), getSolitaryBlockedReason(m, format),
            tf(m->m_tearingState.activelyTearing), getTearingBlockedReason(m, format), rc<uint64_t>(m->m_lastScanout.get()), getDSBlockedReason(m, format), tf(m->m_enabled),
            formatToString(m->m_output->state->state().drmFormat), m->m_mirrorOf ? std::format("{}", m->m_mirrorOf->m_id) : "none", availableModesForOutput(m, format),
            (NCMType::toString(m->m_cmType)), (m->m_sdrBrightness), (m->m_sdrSaturation), (m->m_sdrMinLuminance), (m->m_sdrMaxLuminance), tf(!m->shouldUseSoftwareCursors()),
            StringUtils::backendStr(m->m_output->getBackend()->type()), tf(m->m_output->parsedEDID.hdrMetadata.has_value()),
            tf(m->m_output->parsedEDID.chromaticityCoords.has_value()), tf(m->m_output->parsedEDID.supportsBT2020), tf(m->m_output->vrrCapable));

    } else {
        result += std::format(
            "Monitor {} (ID {}):\n\t{}x{}@{:.5f} at {}x{}\n\tdescription: {}\n\tmake: {}\n\tmodel: {}\n\tphysical size (mm): {}x{}\n\tserial: {}\n\tactive workspace: {} ({})\n\t"
            "special workspace: {} ({})\n\treserved: {} {} {} {}\n\tscale: {}\n\ttransform: {}\n\tfocused: {}\n\t"
            "dpmsStatus: {}\n\tvrr: {}\n\tsolitary: {:x}\n\tsolitaryBlockedBy: {}\n\tactivelyTearing: {}\n\ttearingBlockedBy: {}\n\tdirectScanoutTo: "
            "{:x}\n\tdirectScanoutBlockedBy: {}\n\tdisabled: "
            "{}\n\tcurrentFormat: {}\n\tmirrorOf: "
            "{}\n\tavailableModes: {}\n\tcolorManagementPreset: {}\n\tsdrBrightness: {}\n\tsdrSaturation: {}\n\tsdrMinLuminance: {}\n\tsdrMaxLuminance: "
            "{}\n\thardwareCursorsInUse: {}\n\thardwareDetails:\n\t\tbackend: {}\n\t\thdr: {}\n\t\tchroma: {}\n\t\tbt2020: {}\n\t\tvrrCapable: {}\n\n",
            m->m_name, m->m_id, sc<int>(m->m_pixelSize.x), sc<int>(m->m_pixelSize.y), m->m_refreshRate, sc<int>(m->m_position.x), sc<int>(m->m_position.y), m->m_shortDescription,
            m->m_output->make, m->m_output->model, sc<int>(m->m_output->physicalSize.x), sc<int>(m->m_output->physicalSize.y), m->m_output->serial, m->activeWorkspaceID(),
            (!m->m_activeWorkspace ? "" : m->m_activeWorkspace->m_name), m->activeSpecialWorkspaceID(), (m->m_activeSpecialWorkspace ? m->m_activeSpecialWorkspace->m_name : ""),
            sc<int>(m->m_reservedArea.left()), sc<int>(m->m_reservedArea.top()), sc<int>(m->m_reservedArea.right()), sc<int>(m->m_reservedArea.bottom()), m->m_scale,
            sc<int>(m->m_transform), (m == Desktop::focusState()->monitor() ? "yes" : "no"), sc<int>(m->m_dpmsStatus), m->m_output->state->state().adaptiveSync,
            rc<uint64_t>(m->m_solitaryClient.get()), getSolitaryBlockedReason(m, format), m->m_tearingState.activelyTearing, getTearingBlockedReason(m, format),
            rc<uint64_t>(m->m_lastScanout.get()), getDSBlockedReason(m, format), !m->m_enabled, formatToString(m->m_output->state->state().drmFormat),
            m->m_mirrorOf ? std::format("{}", m->m_mirrorOf->m_id) : "none", availableModesForOutput(m, format), (NCMType::toString(m->m_cmType)), (m->m_sdrBrightness),
            (m->m_sdrSaturation), (m->m_sdrMinLuminance), (m->m_sdrMaxLuminance), (!m->shouldUseSoftwareCursors()), StringUtils::backendStr(m->m_output->getBackend()->type()),
            yn(m->m_output->parsedEDID.hdrMetadata.has_value()), yn(m->m_output->parsedEDID.chromaticityCoords.has_value()), yn(m->m_output->parsedEDID.supportsBT2020),
            yn(m->m_output->vrrCapable));
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

        for (auto const& m : allMonitors ? State::monitorState()->allMonitors() : State::monitorState()->monitors()) {
            result += CCommandFormatter::getMonitorData(m, format);
        }

        trimTrailingComma(result);

        result += "]";
    } else {
        for (auto const& m : allMonitors ? State::monitorState()->allMonitors() : State::monitorState()->monitors()) {
            if (!m->m_output || m->m_id == -1)
                continue;

            result += CCommandFormatter::getMonitorData(m, format);
        }
    }

    return result;
}

static std::string getTagsData(PHLWINDOW w, eHyprCtlOutputFormat format) {
    const auto tags = w->m_ruleApplicator->m_tagKeeper.getTags();

    if (format == eHyprCtlOutputFormat::FORMAT_JSON)
        return std::ranges::fold_left(tags, std::string(),
                                      [](const std::string& a, const std::string& b) { return a.empty() ? std::format("\"{}\"", b) : std::format("{}, \"{}\"", a, b); });
    else
        return std::ranges::fold_left(tags, std::string(), [](const std::string& a, const std::string& b) { return a.empty() ? b : std::format("{}, {}", a, b); });
}

static std::string getGroupedData(PHLWINDOW w, eHyprCtlOutputFormat format) {
    const bool isJson = format == eHyprCtlOutputFormat::FORMAT_JSON;
    if (!w->grouping().group())
        return isJson ? "" : "0";

    std::ostringstream result;

    for (const auto& curr : w->grouping().group()->windows()) {
        if (isJson)
            result << std::format("\"0x{:x}\"", rc<uintptr_t>(curr.get()));
        else
            result << std::format("{:x}", rc<uintptr_t>(curr.get()));

        if (curr != w->grouping().group()->windows().back())
            result << (isJson ? ", " : ",");
    }

    return result.str();
}

std::string CCommandFormatter::getWindowData(PHLWINDOW w, eHyprCtlOutputFormat format) {
    auto getFocusHistoryID = [](PHLWINDOW wnd) -> int {
        const auto& HISTORY = Desktop::History::windowTracker()->fullHistory();
        for (size_t i = 0; i < HISTORY.size(); ++i) {
            if (HISTORY[i].lock() == wnd)
                return HISTORY.size() - i - 1; // reverse order for backwards compat
        }
        return -1;
    };

    const auto METADATA = w->backend().metadata();
    const bool VISIBLE  = w->mapped() && w->acceptsInput() && w->alphaNonZero();

    if (format == eHyprCtlOutputFormat::FORMAT_JSON) {
        return std::format(
            R"#({{
    "address": "0x{:x}",
    "mapped": {},
    "hidden": {},
    "visible": {},
    "acceptsInput": {},
    "at": [{}, {}],
    "size": [{}, {}],
    "workspace": {{
        "id": {},
        "name": "{}"
    }},
    "floating": {},
    "monitor": {},
    "class": "{}",
    "title": "{}",
    "initialClass": "{}",
    "initialTitle": "{}",
    "pid": {},
    "xwayland": {},
    "pinned": {},
    "pinFullscreened": {},
    "fullscreen": {},
    "fullscreenClient": {},
    "fullscreenHandler": "{}",
    "allowedOverFullscreen": {},
    "grouped": [{}],
    "tags": [{}],
    "swallowing": "0x{:x}",
    "focusHistoryID": {},
    "inhibitingIdle": {},
    "xdgTag": "{}",
    "xdgDescription": "{}",
    "contentType": "{}",
    "tearingHint": {},
    "stableId": "{:x}"
}},)#",
            rc<uintptr_t>(w.get()), (w->mapped() ? "true" : "false"), (w->isHidden() ? "true" : "false"), (VISIBLE ? "true" : "false"), (w->acceptsInput() ? "true" : "false"),
            sc<int>(w->position(Desktop::View::IGeometric::GEOMETRIC_GOAL).x), sc<int>(w->position(Desktop::View::IGeometric::GEOMETRIC_GOAL).y),
            sc<int>(w->size(Desktop::View::IGeometric::GEOMETRIC_GOAL).x), sc<int>(w->size(Desktop::View::IGeometric::GEOMETRIC_GOAL).y),
            w->m_workspace ? w->workspaceID() : WORKSPACE_INVALID, escapeJSONStrings(!w->m_workspace ? "" : w->m_workspace->m_name),
            (sc<int>(w->isFloating()) == 1 ? "true" : "false"), w->monitorID(), escapeJSONStrings(w->metadata().appID()), escapeJSONStrings(w->metadata().title()),
            escapeJSONStrings(w->metadata().initialAppID()), escapeJSONStrings(w->metadata().initialTitle()), w->backend().pid(), (w->backend().isX11() ? "true" : "false"),
            ((w->m_state & Desktop::View::WINDOW_STATE_PINNED) ? "true" : "false"), (w->fullscreenPolicy().pinFullscreened() ? "true" : "false"),
            sc<uint8_t>(Fullscreen::controller()->getFullscreenModes(w).internal), sc<uint8_t>(Fullscreen::controller()->getFullscreenModes(w).client),
            escapeJSONStrings(Fullscreen::controller()->getFullscreenHandlerNameAsString(w)), (w->fullscreenPolicy().allowedOverFullscreen() ? "true" : "false"),
            getGroupedData(w, format), getTagsData(w, format), rc<uintptr_t>(w->swallowing().swallowee().get()), getFocusHistoryID(w),
            (g_pInputManager->isWindowInhibiting(w, false) ? "true" : "false"), escapeJSONStrings(METADATA.tag.value_or("")), escapeJSONStrings(METADATA.description.value_or("")),
            escapeJSONStrings(NContentType::toString(w->getContentType())), ((w->m_hints & Desktop::View::WINDOW_HINT_TEAR) ? "true" : "false"), w->metadata().stableID());
    } else {
        return std::format(
            "Window {:x} -> {}:\n\tmapped: {}\n\thidden: {}\n\tvisible: {}\n\tacceptsInput: {}\n\tat: {},{}\n\tsize: {},{}\n\tworkspace: {} ({})\n\tfloating: {}\n\tmonitor: "
            "{}\n\tclass: {}\n\ttitle: "
            "{}\n\tinitialClass: {}\n\tinitialTitle: {}\n\tpid: "
            "{}\n\txwayland: {}\n\tpinned: {}\n\tpinFullscreened: "
            "{}\n\tfullscreen: {}\n\tfullscreenClient: {}\n\tfullscreenHandler: {}\n\tallowedOverFullscreen: {}\n\tgrouped: {}\n\ttags: {}\n\tswallowing: {:x}\n\tfocusHistoryID: "
            "{}\n\tinhibitingIdle: "
            "{}\n\txdgTag: "
            "{}\n\txdgDescription: {}\n\tcontentType: {}\n\ttearingHint: {}\n\tstableID: {:x}\n\n",
            rc<uintptr_t>(w.get()), w->metadata().title(), sc<int>(w->mapped()), sc<int>(w->isHidden()), sc<int>(VISIBLE), sc<int>(w->acceptsInput()),
            sc<int>(w->position(Desktop::View::IGeometric::GEOMETRIC_GOAL).x), sc<int>(w->position(Desktop::View::IGeometric::GEOMETRIC_GOAL).y),
            sc<int>(w->size(Desktop::View::IGeometric::GEOMETRIC_GOAL).x), sc<int>(w->size(Desktop::View::IGeometric::GEOMETRIC_GOAL).y),
            w->m_workspace ? w->workspaceID() : WORKSPACE_INVALID, (!w->m_workspace ? "" : w->m_workspace->m_name), sc<int>(w->isFloating()), w->monitorID(), w->metadata().appID(),
            w->metadata().title(), w->metadata().initialAppID(), w->metadata().initialTitle(), w->backend().pid(), sc<int>(w->backend().isX11()),
            sc<int>(sc<bool>(w->m_state & Desktop::View::WINDOW_STATE_PINNED)), sc<int>(w->fullscreenPolicy().pinFullscreened()),
            sc<uint8_t>(Fullscreen::controller()->getFullscreenModes(w).internal), sc<uint8_t>(Fullscreen::controller()->getFullscreenModes(w).client),
            Fullscreen::controller()->getFullscreenHandlerNameAsString(w), sc<int>(w->fullscreenPolicy().allowedOverFullscreen()), getGroupedData(w, format),
            getTagsData(w, format), rc<uintptr_t>(w->swallowing().swallowee().get()), getFocusHistoryID(w), sc<int>(g_pInputManager->isWindowInhibiting(w, false)),
            METADATA.tag.value_or(""), METADATA.description.value_or(""), NContentType::toString(w->getContentType()),
            sc<int>(sc<bool>(w->m_hints & Desktop::View::WINDOW_HINT_TEAR)), w->metadata().stableID());
    }
}

static std::string clientsRequest(const SRequest& request) {
    const auto  format = request.format;
    std::string result = "";
    if (format == eHyprCtlOutputFormat::FORMAT_JSON) {
        result += "[";

        for (auto const& w : Desktop::windowState()->windows()) {
            if (!w->mapped() && !request.all)
                continue;

            result += CCommandFormatter::getWindowData(w, format);
        }

        trimTrailingComma(result);

        result += "]";
    } else {
        for (auto const& w : Desktop::windowState()->windows()) {
            if (!w->mapped() && !request.all)
                continue;

            result += CCommandFormatter::getWindowData(w, format);
        }

        if (result.empty())
            return "no open windows";
    }
    return result;
}

std::string CCommandFormatter::getWorkspaceData(PHLWORKSPACE w, eHyprCtlOutputFormat format) {
    const auto  PLASTW   = w->getLastFocusedWindow();
    const auto  PMONITOR = w->m_monitor.lock();

    std::string layoutName = "unknown";
    if (w->m_space && w->m_space->algorithm() && w->m_space->algorithm()->tiledAlgo()) {
        const auto& TILED_ALGO = w->m_space->algorithm()->tiledAlgo();
        layoutName             = Layout::Supplementary::algoMatcher()->getNameForTiledAlgo(&typeid(*TILED_ALGO.get()));
    }

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
    "ispersistent": {},
    "tiledLayout": "{}"
}})#",
                           w->m_id, escapeJSONStrings(w->m_name), escapeJSONStrings(PMONITOR ? PMONITOR->m_name : "?"),
                           escapeJSONStrings(PMONITOR ? std::to_string(PMONITOR->m_id) : "null"), w->getWindowCount(),
                           Fullscreen::controller()->hasFullscreen(w) ? "true" : "false", rc<uintptr_t>(PLASTW.get()), PLASTW ? escapeJSONStrings(PLASTW->metadata().title()) : "",
                           w->isPersistent() ? "true" : "false", escapeJSONStrings(layoutName));
    } else {
        return std::format("workspace ID {} ({}) on monitor {}:\n\tmonitorID: {}\n\twindows: {}\n\thasfullscreen: {}\n\tlastwindow: 0x{:x}\n\tlastwindowtitle: {}\n\tispersistent: "
                           "{}\n\ttiledLayout: {}\n\n",
                           w->m_id, w->m_name, PMONITOR ? PMONITOR->m_name : "?", PMONITOR ? std::to_string(PMONITOR->m_id) : "null", w->getWindowCount(),
                           sc<int>(Fullscreen::controller()->hasFullscreen(w)), rc<uintptr_t>(PLASTW.get()), PLASTW ? PLASTW->metadata().title() : "", sc<int>(w->isPersistent()),
                           layoutName);
    }
}

static std::string getWorkspaceRuleData(const Config::CWorkspaceRule& r, eHyprCtlOutputFormat format) {
    const auto boolToString = [](const bool b) -> std::string { return b ? "true" : "false"; };
    if (format == eHyprCtlOutputFormat::FORMAT_JSON) {
        const std::string monitor     = r.m_monitor.empty() ? "" : std::format(",\n    \"monitor\": \"{}\"", escapeJSONStrings(r.m_monitor));
        const std::string enabled     = std::format(",\n    \"enabled\": {}", boolToString(r.isEnabled()));
        const std::string default_    = sc<bool>(r.m_isDefault) ? std::format(",\n    \"default\": {}", boolToString(r.m_isDefault.value())) : "";
        const std::string persistent  = sc<bool>(r.m_isPersistent) ? std::format(",\n    \"persistent\": {}", boolToString(r.m_isPersistent.value())) : "";
        const std::string gapsIn      = sc<bool>(r.m_gapsIn) ?
            std::format(",\n    \"gapsIn\": [{}, {}, {}, {}]", r.m_gapsIn.value().m_top, r.m_gapsIn.value().m_right, r.m_gapsIn.value().m_bottom, r.m_gapsIn.value().m_left) :
            "";
        const std::string gapsOut     = sc<bool>(r.m_gapsOut) ?
            std::format(",\n    \"gapsOut\": [{}, {}, {}, {}]", r.m_gapsOut.value().m_top, r.m_gapsOut.value().m_right, r.m_gapsOut.value().m_bottom, r.m_gapsOut.value().m_left) :
            "";
        const std::string borderSize  = sc<bool>(r.m_borderSize) ? std::format(",\n    \"borderSize\": {}", r.m_borderSize.value()) : "";
        const std::string border      = sc<bool>(r.m_noBorder) ? std::format(",\n    \"border\": {}", boolToString(!r.m_noBorder.value())) : "";
        const std::string rounding    = sc<bool>(r.m_noRounding) ? std::format(",\n    \"rounding\": {}", boolToString(!r.m_noRounding.value())) : "";
        const std::string decorate    = sc<bool>(r.m_decorate) ? std::format(",\n    \"decorate\": {}", boolToString(r.m_decorate.value())) : "";
        const std::string shadow      = sc<bool>(r.m_noShadow) ? std::format(",\n    \"shadow\": {}", boolToString(!r.m_noShadow.value())) : "";
        const std::string defaultName = r.m_defaultName.has_value() ? std::format(",\n    \"defaultName\": \"{}\"", escapeJSONStrings(r.m_defaultName.value())) : "";
        const std::string onCreatedEmpty =
            r.m_onCreatedEmptyRunCmd.has_value() ? std::format(",\n    \"onCreatedEmpty\": \"{}\"", escapeJSONStrings(r.m_onCreatedEmptyRunCmd.value())) : "";

        std::string result = std::format(R"#({{
    "workspaceString": "{}"{}{}{}{}{}{}{}{}{}{}{}{}{}
}})#",
                                         escapeJSONStrings(r.m_workspaceString), enabled, monitor, default_, persistent, gapsIn, gapsOut, borderSize, border, rounding, decorate,
                                         shadow, defaultName, onCreatedEmpty);

        return result;
    } else {
        const std::string monitor        = std::format("\tmonitor: {}\n", r.m_monitor.empty() ? "<unset>" : escapeJSONStrings(r.m_monitor));
        const std::string enabled        = std::format("\tenabled: {}\n", boolToString(r.isEnabled()));
        const std::string default_       = std::format("\tdefault: {}\n", sc<bool>(r.m_isDefault) ? boolToString(r.m_isDefault.value()) : "<unset>");
        const std::string persistent     = std::format("\tpersistent: {}\n", sc<bool>(r.m_isPersistent) ? boolToString(r.m_isPersistent.value()) : "<unset>");
        const std::string gapsIn         = sc<bool>(r.m_gapsIn) ?
            std::format("\tgapsIn: {} {} {} {}\n", std::to_string(r.m_gapsIn.value().m_top), std::to_string(r.m_gapsIn.value().m_right),
                        std::to_string(r.m_gapsIn.value().m_bottom), std::to_string(r.m_gapsIn.value().m_left)) :
            std::format("\tgapsIn: <unset>\n");
        const std::string gapsOut        = sc<bool>(r.m_gapsOut) ?
            std::format("\tgapsOut: {} {} {} {}\n", std::to_string(r.m_gapsOut.value().m_top), std::to_string(r.m_gapsOut.value().m_right),
                        std::to_string(r.m_gapsOut.value().m_bottom), std::to_string(r.m_gapsOut.value().m_left)) :
            std::format("\tgapsOut: <unset>\n");
        const std::string borderSize     = std::format("\tborderSize: {}\n", sc<bool>(r.m_borderSize) ? std::to_string(r.m_borderSize.value()) : "<unset>");
        const std::string border         = std::format("\tborder: {}\n", sc<bool>(r.m_noBorder) ? boolToString(!r.m_noBorder.value()) : "<unset>");
        const std::string rounding       = std::format("\trounding: {}\n", sc<bool>(r.m_noRounding) ? boolToString(!r.m_noRounding.value()) : "<unset>");
        const std::string decorate       = std::format("\tdecorate: {}\n", sc<bool>(r.m_decorate) ? boolToString(r.m_decorate.value()) : "<unset>");
        const std::string shadow         = std::format("\tshadow: {}\n", sc<bool>(r.m_noShadow) ? boolToString(!r.m_noShadow.value()) : "<unset>");
        const std::string defaultName    = std::format("\tdefaultName: {}\n", r.m_defaultName.value_or("<unset>"));
        const std::string onCreatedEmpty = std::format("\tonCreatedEmpty: {}\n", r.m_onCreatedEmptyRunCmd.value_or("<unset>"));

        std::string result = std::format("Workspace rule {}:\n{}{}{}{}{}{}{}{}{}{}{}{}{}\n", escapeJSONStrings(r.m_workspaceString), enabled, monitor, default_, persistent, gapsIn,
                                         gapsOut, borderSize, border, rounding, decorate, shadow, defaultName, onCreatedEmpty);

        return result;
    }
}

static std::string activeWorkspaceRequest(eHyprCtlOutputFormat format, std::string request) {
    if (!Desktop::focusState()->monitor())
        return "unsafe state";

    std::string result = "";
    auto        w      = Desktop::focusState()->monitor()->m_activeWorkspace;

    if (!valid(w))
        return "internal error";

    return CCommandFormatter::getWorkspaceData(w, format);
}

static std::string workspacesRequest(eHyprCtlOutputFormat format, std::string request) {
    std::string result = "";

    if (format == eHyprCtlOutputFormat::FORMAT_JSON) {
        result += "[";
        for (auto const& w : State::workspaceState()->workspaces()) {
            result += CCommandFormatter::getWorkspaceData(w.lock(), format);
            result += ",";
        }

        trimTrailingComma(result);
        result += "]";
    } else {
        for (auto const& w : State::workspaceState()->workspaces()) {
            result += CCommandFormatter::getWorkspaceData(w.lock(), format);
        }
    }

    return result;
}

static std::string workspaceRulesRequest(eHyprCtlOutputFormat format, std::string request) {
    std::string result = "";
    if (format == eHyprCtlOutputFormat::FORMAT_JSON) {
        result += "[";
        for (auto const& r : Config::workspaceRuleMgr()->getAllWorkspaceRules()) {
            result += getWorkspaceRuleData(*r, format);
            result += ",";
        }

        trimTrailingComma(result);
        result += "]";
    } else {
        for (auto const& r : Config::workspaceRuleMgr()->getAllWorkspaceRules()) {
            result += getWorkspaceRuleData(*r, format);
        }
    }

    return result;
}

static std::string activeWindowRequest(eHyprCtlOutputFormat format, std::string request) {
    const auto PWINDOW = Desktop::focusState()->window();

    if (!validMapped(PWINDOW))
        return format == eHyprCtlOutputFormat::FORMAT_JSON ? "{}" : "Invalid";

    auto result = CCommandFormatter::getWindowData(PWINDOW, format);

    if (format == eHyprCtlOutputFormat::FORMAT_JSON)
        result.pop_back();

    return result;
}

static std::string layersRequest(eHyprCtlOutputFormat format, std::string request) {
    std::string result = "";

    if (format == eHyprCtlOutputFormat::FORMAT_JSON) {
        result += "{\n";

        for (auto const& mon : State::monitorState()->monitors()) {
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
                    "alpha": {},
                    "namespace": "{}",
                    "pid": {}
                }},)#",
                        rc<uintptr_t>(layer.get()), layer->m_geometry.x, layer->m_geometry.y, layer->m_geometry.width, layer->m_geometry.height,
                        std::clamp(sc<double>(layer->alpha().goal()), 0.0, 1.0), escapeJSONStrings(layer->m_namespace), layer->getPID());
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
        for (auto const& mon : State::monitorState()->monitors()) {
            result += std::format("Monitor {}:\n", mon->m_name);
            int                                     layerLevel = 0;
            static const std::array<std::string, 4> levelNames = {"background", "bottom", "top", "overlay"};
            for (auto const& level : mon->m_layerSurfaceLayers) {
                result += std::format("\tLayer level {} ({}):\n", layerLevel, levelNames[layerLevel]);

                for (auto const& layer : level) {
                    result += std::format("\t\tLayer {:x}: xywh: {} {} {} {}, a: {}, namespace: {}, pid: {}\n", rc<uintptr_t>(layer.get()), layer->m_geometry.x,
                                          layer->m_geometry.y, layer->m_geometry.width, layer->m_geometry.height, std::clamp(sc<double>(layer->alpha().goal()), 0.0, 1.0),
                                          layer->m_namespace, layer->getPID());
                }

                layerLevel++;
            }
            result += "\n\n";
        }
    }

    return result;
}

static std::string configErrorsRequest(eHyprCtlOutputFormat format, std::string request) {
    std::string result     = "";
    std::string currErrors = Config::mgr()->getErrors();
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
        "type": "tabletTool"
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

        for (auto const& ac : Config::animationTree()->getAnimationConfig()) {
            ret += std::format("\n\tname: {}\n\t\toverridden: {}\n\t\tbezier: {}\n\t\tenabled: {}\n\t\tspeed: {:.2f}\n\t\tstyle: {}\n", ac.first, sc<int>(ac.second->overridden),
                               ac.second->internalBezier, ac.second->internalEnabled, ac.second->internalSpeed, ac.second->internalStyle);
        }

        ret += "beziers:\n";

        for (auto const& bz : Animation::mgr()->getAllBeziers()) {
            auto& controlPoints = bz.second->getControlPoints();
            ret += std::format("\n\tname: {}\n\t\tX0: {:.2f}\n\t\tY0: {:.2f}\n\t\tX1: {:.2f}\n\t\tY1: {:.2f}", bz.first, controlPoints[1].x, controlPoints[1].y, controlPoints[2].x,
                               controlPoints[2].y);
        }
    } else {
        // json

        ret += "[[";
        for (auto const& ac : Config::animationTree()->getAnimationConfig()) {
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

        for (auto const& bz : Animation::mgr()->getAllBeziers()) {
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
        result += escapeJSONStrings(Log::logger->rolling());
        result += "\"]";
    } else
        result = Log::logger->rolling();

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
                               escapeJSONStrings(std::format("{}:{}", sh.appid, sh.id)), escapeJSONStrings(sh.description));
        }
        trimTrailingComma(ret);
        ret += "]\n";
    }

    return ret;
}

static std::string bindFlagNames(const Keybinds::CBind& bind) {
    static constexpr auto FLAGS = std::to_array<std::pair<Keybinds::eBindFlags, std::string_view>>({
        {Keybinds::BIND_FLAG_LOCKED, "locked"},
        {Keybinds::BIND_FLAG_RELEASE, "release"},
        {Keybinds::BIND_FLAG_REPEAT, "repeat"},
        {Keybinds::BIND_FLAG_LONG_PRESS, "long_press"},
        {Keybinds::BIND_FLAG_NON_CONSUMING, "non_consuming"},
        {Keybinds::BIND_FLAG_AUTO_CONSUMING, "auto_consuming"},
        {Keybinds::BIND_FLAG_TRANSPARENT, "transparent"},
        {Keybinds::BIND_FLAG_IGNORE_MODS, "ignore_mods"},
        {Keybinds::BIND_FLAG_DONT_INHIBIT, "dont_inhibit"},
        {Keybinds::BIND_FLAG_CLICK, "click"},
        {Keybinds::BIND_FLAG_DRAG, "drag"},
        {Keybinds::BIND_FLAG_SUBMAP_UNIVERSAL, "submap_universal"},
        {Keybinds::BIND_FLAG_ALLOW_INPUT_CAPTURE, "allow_input_capture"},
        {Keybinds::BIND_FLAG_DEVICE_INCLUSIVE, "device_inclusive"},
        {Keybinds::BIND_FLAG_CATCH_ALL, "catch_all"},
        {Keybinds::BIND_FLAG_MOUSE, "mouse"},
    });

    std::string           result;
    for (const auto& [flag, name] : FLAGS) {
        if (!bind.hasFlag(flag))
            continue;

        if (!result.empty())
            result += ", ";
        result += name;
    }

    return result;
}

static std::string bindsRequest(eHyprCtlOutputFormat format, std::string request) {
    std::string ret = "";
    if (format == eHyprCtlOutputFormat::FORMAT_NORMAL) {
        for (const auto& kb : Keybinds::mgr()->registry().binds()) {
            const auto& METADATA  = kb->metadata();
            const auto  KEYS      = kb->keys();
            const auto  KEY_NAMES = kb->keyNames();
            const auto  KEYCODE   = KEYS.empty() ? std::nullopt : KEYS.back().keycode();
            const auto  KEY       = kb->hasFlag(Keybinds::BIND_FLAG_CATCH_ALL) || KEYCODE || KEY_NAMES.empty() ? std::string{} : KEY_NAMES.back();

            ret += std::format("bind\n\tflags: {}\n\tmodmask: {}\n\tsubmap: {}\n\tkey: {}\n\tkeycode: {}\n\tcatchall: {}\n\tdescription: {}\n\tdispatcher: {}\n\targ: {}\n\n",
                               bindFlagNames(*kb), sc<uint32_t>(kb->modifierMask()), METADATA.submap, KEY.empty() ? METADATA.displayKey : KEY, KEYCODE.value_or(0),
                               kb->hasFlag(Keybinds::BIND_FLAG_CATCH_ALL), METADATA.description.value_or(""), METADATA.handler, METADATA.argument);
        }
    } else {
        // json
        ret += "[";
        for (const auto& kb : Keybinds::mgr()->registry().binds()) {
            const auto& METADATA  = kb->metadata();
            const auto  KEYS      = kb->keys();
            const auto  KEY_NAMES = kb->keyNames();
            const auto  KEYCODE   = KEYS.empty() ? std::nullopt : KEYS.back().keycode();
            const auto  KEY       = kb->hasFlag(Keybinds::BIND_FLAG_CATCH_ALL) || KEYCODE || KEY_NAMES.empty() ? std::string{} : KEY_NAMES.back();

            ret += std::format(
                R"#(
{{
    "locked": {},
    "mouse": {},
    "release": {},
    "repeat": {},
    "longPress": {},
    "non_consuming": {},
    "auto_consuming": {},
    "has_description": {},
    "modmask": {},
    "submap": "{}",
    "submap_universal": "{}",
    "key": "{}",
    "keycode": {},
    "catch_all": {},
    "description": "{}",
    "allow_input_capture": {},
    "dispatcher": "{}",
    "arg": "{}"
}},)#",
                kb->hasFlag(Keybinds::BIND_FLAG_LOCKED) ? "true" : "false", kb->hasFlag(Keybinds::BIND_FLAG_MOUSE) ? "true" : "false",
                kb->hasFlag(Keybinds::BIND_FLAG_RELEASE) ? "true" : "false", kb->hasFlag(Keybinds::BIND_FLAG_REPEAT) ? "true" : "false",
                kb->hasFlag(Keybinds::BIND_FLAG_LONG_PRESS) ? "true" : "false", kb->hasFlag(Keybinds::BIND_FLAG_NON_CONSUMING) ? "true" : "false",
                kb->hasFlag(Keybinds::BIND_FLAG_AUTO_CONSUMING) ? "true" : "false", METADATA.description ? "true" : "false", sc<uint32_t>(kb->modifierMask()),
                escapeJSONStrings(METADATA.submap), kb->hasFlag(Keybinds::BIND_FLAG_SUBMAP_UNIVERSAL) ? "true" : "false", escapeJSONStrings(KEY), KEYCODE.value_or(0),
                kb->hasFlag(Keybinds::BIND_FLAG_CATCH_ALL) ? "true" : "false", escapeJSONStrings(METADATA.description.value_or("")),
                kb->hasFlag(Keybinds::BIND_FLAG_ALLOW_INPUT_CAPTURE) ? "true" : "false", escapeJSONStrings(METADATA.handler), escapeJSONStrings(METADATA.argument));
        }
        trimTrailingComma(ret);
        ret += "]";
    }

    return ret;
}

std::string IPC::Socket1::version(eOutputFormat format) {
    return Helpers::SystemInfo::getVersion(format);
}

static std::string versionRequest(eHyprCtlOutputFormat format, std::string request) {
    return version(format);
}

static std::string statusRequest(eHyprCtlOutputFormat format, std::string request) {
    return Helpers::SystemInfo::getStatus(format);
}

static std::string deprecatedConfigRequest(eHyprCtlOutputFormat format, std::string request) {
    const auto  OPTS = Config::mgr()->deprecationNotices();

    std::string ret = "";

    if (format == IPC::Socket1::FORMAT_JSON) {
        ret += "[";
        for (const auto& o : OPTS) {
            ret += std::format("\"{}\", ", escapeJSONStrings(o));
        }
        if (!OPTS.empty()) {
            ret.pop_back();
            ret.pop_back();
        }

        ret += "]";
    } else {
        if (!OPTS.empty()) {
            for (const auto& o : OPTS) {
                ret += std::format("{}\n", o);
            }
        } else
            ret = "all good!";
    }

    return ret;
}

std::string IPC::Socket1::systemInfo(eOutputFormat format, bool includeConfig) {
    auto result = Helpers::SystemInfo::getSystemInfo();

    if (includeConfig) {
        result += "\n\n======Config-Start======\n";
        result += Config::mgr()->getConfigString();
        result += "\n======Config-End========\n";
    }

    return result;
}

static std::string systemInfoRequest(const SRequest& request) {
    return systemInfo(request.format, request.includeConfig);
}

static std::string evalRequest(eHyprCtlOutputFormat format, std::string request) {
    if (Config::mgr()->type() != Config::CONFIG_LUA)
        return "eval is only supported with the lua config manager";

    auto luaMgr = dynamicPointerCast<Config::Lua::CConfigManager>(WP<Config::IConfigManager>(Config::mgr()));

    // strip the command name ("eval ") from the request
    auto code = request.substr(request.find_first_of(' ') + 1);

    auto err = luaMgr->eval(code, request.starts_with("repl "));
    if (err)
        return *err;

    return "ok";
}

static std::string dispatchRequest(eHyprCtlOutputFormat format, std::string in) {
    // get rid of the dispatch keyword
    in = in.substr(in.find_first_of(' ') + 1);

    if (Config::mgr()->type() == Config::CONFIG_LUA) {
        // For lua, this is just a wrapper for `eval("hl.dispatch(in)")
        std::string evalStr = std::format("return hl.dispatch({})", in);
        auto        luaMgr  = dynamicPointerCast<Config::Lua::CConfigManager>(WP<Config::IConfigManager>(Config::mgr()));
        auto        ret     = luaMgr->eval(evalStr).value_or("ok");

        if (ret.starts_with("ok") || in.contains("(") /* this likely means the user is passing a valid lua dispatch string */)
            return ret;

        // the user likely is trying to dispatch old hyprlang stuff via lua, let them know
        return std::format("{}\n\n → Note: dispatch in lua is a shorthand for hl.dispatch(...), your syntax might need to be updated.", ret);
    }

    return "current config provider doesn't support dispatch";
}

static std::string reloadRequest(eHyprCtlOutputFormat format, std::string request) {

    if (request.ends_with("full-reset")) {
        // perform a full config reset. First, reset config manager and load it anew. Then,
        // flush all config value caches. This will momentarily degrade performance
        // but a config load will be slow enough for this not to matter probably
        Config::mgr().reset();
        Config::Supplementary::Jeremy::flushCachedCfgPath();
        Config::initConfigManager();
        CConfigValueBase::flushCaches();
        Config::mgr()->init();
        CConfigValueBase::flushCaches();

        return "ok";
    }

    Config::mgr()->reload();

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
    const auto CURSORPOS = Pointer::mgr()->untransformedPosition().floor();

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

static std::string dispatchSetCursor(eHyprCtlOutputFormat format, std::string request) {
    CVarList    vars(request, 0, ' ');

    const auto  SIZESTR = vars[vars.size() - 1];
    std::string theme   = "";
    for (size_t i = 1; i < vars.size() - 1; ++i)
        theme += std::format("{} ", vars[i]);
    if (!theme.empty())
        theme.pop_back();

    int size = 0;
    try {
        size = std::stoi(SIZESTR);
    } catch (...) { return "size not int"; }

    if (size <= 0)
        return "size not positive";

    if (!Pointer::Cursor::mgr()->changeTheme(theme, size))
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
                return std::format("layout idx out of range of {}", LAYOUTS);
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
                result += std::format("{}\n", *res);
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
        ErrorOverlay::overlay()->destroy();

        if (vars.size() == 2 && !vars[1].contains("dis"))
            return "var 1 not color or disable";

        return "ok";
    }

    const CHyprColor COLOR = Config::ParserUtils::parseColor(vars[1]).value_or(0);

    for (size_t i = 2; i < vars.size(); ++i)
        errorMessage += vars[i] + ' ';

    if (errorMessage.empty()) {
        ErrorOverlay::overlay()->destroy();
    } else {
        errorMessage.pop_back(); // pop last space
        ErrorOverlay::overlay()->queueCreate(errorMessage, COLOR);
    }

    return "ok";
}

static std::string dispatchGetProp(eHyprCtlOutputFormat format, std::string request) {
    CVarList vars(request, 0, ' ');

    if (vars.size() < 3)
        return "not enough args";

    const auto WINREGEX = vars[1];
    const auto PROP     = vars[2];

    const auto PWINDOW = Desktop::viewState()->query().selector(WINREGEX).runWindow();

    if (!PWINDOW)
        return "window not found";

    const bool FORMNORM = format == IPC::Socket1::FORMAT_NORMAL;

    auto       sizeToString = [&](bool max) -> std::string {
        auto sizeValue = PWINDOW->m_ruleApplicator->minSize().valueOr(Vector2D(MIN_WINDOW_SIZE, MIN_WINDOW_SIZE));
        if (max)
            sizeValue = PWINDOW->m_ruleApplicator->maxSize().valueOr(Vector2D(INFINITY, INFINITY));

        if (FORMNORM)
            return std::format("{} {}", sizeValue.x, sizeValue.y);
        else {
            std::string xSizeString = (sizeValue.x != INFINITY) ? std::to_string(sizeValue.x) : "null";
            std::string ySizeString = (sizeValue.y != INFINITY) ? std::to_string(sizeValue.y) : "null";
            return std::format(R"({{"{}": [{},{}]}})", PROP, xSizeString, ySizeString);
        }
    };

    auto alphaToString = [&](Desktop::Types::COverridableVar<Desktop::Types::SAlphaValue>& alpha, bool getAlpha) -> std::string {
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
        static auto PACTIVECOL              = CConfigValue<Config::IComplexConfigValue>("general:col.active_border");
        static auto PINACTIVECOL            = CConfigValue<Config::IComplexConfigValue>("general:col.inactive_border");
        static auto PNOGROUPACTIVECOL       = CConfigValue<Config::IComplexConfigValue>("general:col.nogroup_border_active");
        static auto PNOGROUPINACTIVECOL     = CConfigValue<Config::IComplexConfigValue>("general:col.nogroup_border");
        static auto PGROUPACTIVECOL         = CConfigValue<Config::IComplexConfigValue>("group:col.border_active");
        static auto PGROUPINACTIVECOL       = CConfigValue<Config::IComplexConfigValue>("group:col.border_inactive");
        static auto PGROUPACTIVELOCKEDCOL   = CConfigValue<Config::IComplexConfigValue>("group:col.border_locked_active");
        static auto PGROUPINACTIVELOCKEDCOL = CConfigValue<Config::IComplexConfigValue>("group:col.border_locked_inactive");

        const bool  GROUPLOCKED = PWINDOW->grouping().group() ? PWINDOW->grouping().group()->locked() : false;

        if (active) {
            auto* const       ACTIVECOL            = (Config::CGradientValueData*)(PACTIVECOL.ptr());
            auto* const       NOGROUPACTIVECOL     = (Config::CGradientValueData*)(PNOGROUPACTIVECOL.ptr());
            auto* const       GROUPACTIVECOL       = (Config::CGradientValueData*)(PGROUPACTIVECOL.ptr());
            auto* const       GROUPACTIVELOCKEDCOL = (Config::CGradientValueData*)(PGROUPACTIVELOCKEDCOL.ptr());
            const auto* const ACTIVECOLOR          = !PWINDOW->grouping().group() ? (!(PWINDOW->grouping().rules() & Desktop::View::GROUP_DENY) ? ACTIVECOL : NOGROUPACTIVECOL) :
                                                                                    (GROUPLOCKED ? GROUPACTIVELOCKEDCOL : GROUPACTIVECOL);

            std::string       borderColorString = PWINDOW->m_ruleApplicator->activeBorderColor().valueOr(*ACTIVECOLOR).toString();
            if (FORMNORM)
                return borderColorString;
            else
                return std::format(R"({{"{}": "{}"}})", PROP, borderColorString);
        } else {
            auto* const       INACTIVECOL            = (Config::CGradientValueData*)(PINACTIVECOL.ptr());
            auto* const       NOGROUPINACTIVECOL     = (Config::CGradientValueData*)(PNOGROUPINACTIVECOL.ptr());
            auto* const       GROUPINACTIVECOL       = (Config::CGradientValueData*)(PGROUPINACTIVECOL.ptr());
            auto* const       GROUPINACTIVELOCKEDCOL = (Config::CGradientValueData*)(PGROUPINACTIVELOCKEDCOL.ptr());
            const auto* const INACTIVECOLOR = !PWINDOW->grouping().group() ? (!(PWINDOW->grouping().rules() & Desktop::View::GROUP_DENY) ? INACTIVECOL : NOGROUPINACTIVECOL) :
                                                                             (GROUPLOCKED ? GROUPINACTIVELOCKEDCOL : GROUPINACTIVECOL);

            std::string       borderColorString = PWINDOW->m_ruleApplicator->inactiveBorderColor().valueOr(*INACTIVECOLOR).toString();
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

    if (PROP == "animation") {
        auto& animationStyle = PWINDOW->m_ruleApplicator->animationStyle();
        if (FORMNORM)
            return animationStyle.valueOr("(unset)");
        else
            return std::format(R"({{"{}": "{}"}})", PROP, animationStyle.valueOr(""));
    } else if (PROP == "max_size")
        return sizeToString(true);
    else if (PROP == "min_size")
        return sizeToString(false);
    else if (PROP == "opacity")
        return alphaToString(PWINDOW->m_ruleApplicator->alpha(), true);
    else if (PROP == "opacity_inactive")
        return alphaToString(PWINDOW->m_ruleApplicator->alphaInactive(), true);
    else if (PROP == "opacity_fullscreen")
        return alphaToString(PWINDOW->m_ruleApplicator->alphaFullscreen(), true);
    else if (PROP == "opacity_override")
        return alphaToString(PWINDOW->m_ruleApplicator->alpha(), false);
    else if (PROP == "opacity_inactive_override")
        return alphaToString(PWINDOW->m_ruleApplicator->alphaInactive(), false);
    else if (PROP == "opacity_fullscreen_override")
        return alphaToString(PWINDOW->m_ruleApplicator->alphaFullscreen(), false);
    else if (PROP == "active_border_color")
        return borderColorToString(true);
    else if (PROP == "inactive_border_color")
        return borderColorToString(false);
    else if (PROP == "allows_input")
        return windowPropToString(PWINDOW->m_ruleApplicator->allowsInput());
    else if (PROP == "decorate")
        return windowPropToString(PWINDOW->m_ruleApplicator->decorate());
    else if (PROP == "focus_on_activate")
        return windowPropToString(PWINDOW->m_ruleApplicator->focusOnActivate());
    else if (PROP == "keep_aspect_ratio")
        return windowPropToString(PWINDOW->m_ruleApplicator->keepAspectRatio());
    else if (PROP == "nearest_neighbor")
        return windowPropToString(PWINDOW->m_ruleApplicator->nearestNeighbor());
    else if (PROP == "no_anim")
        return windowPropToString(PWINDOW->m_ruleApplicator->noAnim());
    else if (PROP == "no_blur")
        return windowPropToString(PWINDOW->m_ruleApplicator->noBlur());
    else if (PROP == "no_dim")
        return windowPropToString(PWINDOW->m_ruleApplicator->noDim());
    else if (PROP == "no_focus")
        return windowPropToString(PWINDOW->m_ruleApplicator->noFocus());
    else if (PROP == "no_max_size")
        return windowPropToString(PWINDOW->m_ruleApplicator->noMaxSize());
    else if (PROP == "no_shadow")
        return windowPropToString(PWINDOW->m_ruleApplicator->noShadow());
    else if (PROP == "no_glow")
        return windowPropToString(PWINDOW->m_ruleApplicator->noGlow());
    else if (PROP == "no_wobble")
        return windowPropToString(PWINDOW->m_ruleApplicator->noWobble());
    else if (PROP == "no_shortcuts_inhibit")
        return windowPropToString(PWINDOW->m_ruleApplicator->noShortcutsInhibit());
    else if (PROP == "opaque")
        return windowPropToString(PWINDOW->m_ruleApplicator->opaque());
    else if (PROP == "dim_around")
        return windowPropToString(PWINDOW->m_ruleApplicator->dimAround());
    else if (PROP == "force_rgbx")
        return windowPropToString(PWINDOW->m_ruleApplicator->RGBX());
    else if (PROP == "sync_fullscreen")
        return windowPropToString(PWINDOW->m_ruleApplicator->syncFullscreen());
    else if (PROP == "immediate")
        return windowPropToString(PWINDOW->m_ruleApplicator->tearing());
    else if (PROP == "xray")
        return windowPropToString(PWINDOW->m_ruleApplicator->xray());
    else if (PROP == "render_unfocused")
        return windowPropToString(PWINDOW->m_ruleApplicator->renderUnfocused());
    else if (PROP == "no_follow_mouse")
        return windowPropToString(PWINDOW->m_ruleApplicator->noFollowMouse());
    else if (PROP == "no_screen_share")
        return windowPropToString(PWINDOW->m_ruleApplicator->noScreenShare());
    else if (PROP == "no_vrr")
        return windowPropToString(PWINDOW->m_ruleApplicator->noVRR());
    else if (PROP == "no_auto_hdr")
        return windowPropToString(PWINDOW->m_ruleApplicator->noAutoHDR());
    else if (PROP == "persistent_size")
        return windowPropToString(PWINDOW->m_ruleApplicator->persistentSize());
    else if (PROP == "stay_focused")
        return windowPropToString(PWINDOW->m_ruleApplicator->stayFocused());
    else if (PROP == "idle_inhibit")
        return windowPropToString(PWINDOW->m_ruleApplicator->idleInhibitMode());
    else if (PROP == "border_size")
        return windowPropToString(PWINDOW->m_ruleApplicator->borderSize());
    else if (PROP == "rounding")
        return windowPropToString(PWINDOW->m_ruleApplicator->rounding());
    else if (PROP == "rounding_power")
        return windowPropToString(PWINDOW->m_ruleApplicator->roundingPower());
    else if (PROP == "scroll_mouse")
        return windowPropToString(PWINDOW->m_ruleApplicator->scrollMouse());
    else if (PROP == "scroll_touchpad")
        return windowPropToString(PWINDOW->m_ruleApplicator->scrollTouchpad());

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

    const auto VAR = Config::mgr()->getConfigValue(curitem);

    if (!VAR.dataptr)
        return "no such option";

    const auto VAL  = VAR.dataptr;
    const auto TYPE = std::type_index(*VAR.type);

    if (format == FORMAT_NORMAL) {
        if (TYPE == typeid(Config::INTEGER))
            return std::format("int: {}\nset: {}", **rc<Config::INTEGER* const*>(VAL), VAR.setByUser);
        else if (TYPE == typeid(Config::BOOL))
            return std::format("bool: {}\nset: {}", **rc<Config::BOOL* const*>(VAL), VAR.setByUser);
        else if (TYPE == typeid(Config::FLOAT))
            return std::format("float: {:2f}\nset: {}", **rc<Config::FLOAT* const*>(VAL), VAR.setByUser);
        else if (TYPE == typeid(Config::VEC2))
            return std::format("vec2: [{}, {}]\nset: {}", (*rc<Config::VEC2* const*>(VAL))->x, (*rc<Config::VEC2* const*>(VAL))->y, VAR.setByUser);
        else if (TYPE == typeid(Hyprlang::VEC2))
            return std::format("vec2: [{}, {}]\nset: {}", (*rc<Config::VEC2* const*>(VAL))->x, (*rc<Config::VEC2* const*>(VAL))->y, VAR.setByUser);
        else if (TYPE == typeid(Hyprlang::STRING))
            return std::format("str: {}\nset: {}", *rc<Hyprlang::STRING const*>(VAL), VAR.setByUser);
        else if (TYPE == typeid(Config::STRING))
            return std::format("str: {}\nset: {}", **rc<Config::STRING* const*>(VAL), VAR.setByUser);
        else if (TYPE == typeid(void*))
            return std::format("custom type: {}\nset: {}", rc<Config::IComplexConfigValue*>((*rc<Hyprlang::CUSTOMTYPE* const*>(VAL))->getData())->toString(), VAR.setByUser);
        else if (TYPE == typeid(Config::IComplexConfigValue))
            return std::format("custom type: {}\nset: {}", (*rc<Config::IComplexConfigValue* const*>(VAL))->toString(), VAR.setByUser);
        else if (TYPE == typeid(Config::CCssGapData))
            return std::format("css gap data: {}\nset: {}", (*rc<Config::CCssGapData* const*>(VAL))->toString(), VAR.setByUser);
        else if (TYPE == typeid(Config::CGradientValueData))
            return std::format("gradient data: {}\nset: {}", (*rc<Config::CGradientValueData* const*>(VAL))->toString(), VAR.setByUser);
        else if (TYPE == typeid(Config::CFontWeightConfigValueData))
            return std::format("font weight data: {}\nset: {}", (*rc<Config::CFontWeightConfigValueData* const*>(VAL))->toString(), VAR.setByUser);
    } else {
        if (TYPE == typeid(Config::INTEGER))
            return std::format(R"({{"option": "{}", "int": {}, "set": {} }})", curitem, **rc<Config::INTEGER* const*>(VAL), VAR.setByUser);
        else if (TYPE == typeid(Config::BOOL))
            return std::format(R"({{"option": "{}", "bool": {}, "set": {} }})", curitem, (**rc<Config::BOOL* const*>(VAL)) ? "true" : "false", VAR.setByUser);
        else if (TYPE == typeid(Config::FLOAT))
            return std::format(R"({{"option": "{}", "float": {:2f}, "set": {} }})", curitem, **rc<Config::FLOAT* const*>(VAL), VAR.setByUser);
        else if (TYPE == typeid(Config::VEC2))
            return std::format(R"({{"option": "{}", "vec2": [{},{}], "set": {} }})", curitem, (*rc<Config::VEC2* const*>(VAL))->x, (*rc<Config::VEC2* const*>(VAL))->y,
                               VAR.setByUser);
        else if (TYPE == typeid(Hyprlang::VEC2))
            return std::format(R"({{"option": "{}", "vec2": [{},{}], "set": {} }})", curitem, (*rc<Config::VEC2* const*>(VAL))->x, (*rc<Config::VEC2* const*>(VAL))->y,
                               VAR.setByUser);
        else if (TYPE == typeid(Hyprlang::STRING))
            return std::format(R"({{"option": "{}", "str": "{}", "set": {} }})", curitem, escapeJSONStrings(*rc<Hyprlang::STRING const*>(VAL)), VAR.setByUser);
        else if (TYPE == typeid(Config::STRING))
            return std::format(R"({{"option": "{}", "str": "{}", "set": {} }})", curitem, **rc<Config::STRING* const*>(VAL), VAR.setByUser);
        else if (TYPE == typeid(void*))
            return std::format(R"({{"option": "{}", "custom": "{}", "set": {} }})", curitem,
                               rc<Config::IComplexConfigValue*>((*rc<Hyprlang::CUSTOMTYPE* const*>(VAL))->getData())->toString(), VAR.setByUser);
        else if (TYPE == typeid(Config::IComplexConfigValue))
            return std::format(R"({{"option": "{}", "custom": "{}", "set": {} }})", curitem, (*rc<Config::IComplexConfigValue* const*>(VAL))->toString(), VAR.setByUser);
        else if (TYPE == typeid(Config::CCssGapData))
            return std::format(R"({{"option": "{}", "css": "{}", "set": {} }})", curitem, (*rc<Config::CCssGapData* const*>(VAL))->toString(), VAR.setByUser);
        else if (TYPE == typeid(Config::CGradientValueData))
            return std::format(R"({{"option": "{}", "gradient": "{}", "set": {} }})", curitem, (*rc<Config::CGradientValueData* const*>(VAL))->toString(), VAR.setByUser);
        else if (TYPE == typeid(Config::CFontWeightConfigValueData))
            return std::format(R"({{"option": "{}", "font_weight": "{}", "set": {} }})", curitem, (*rc<Config::CFontWeightConfigValueData* const*>(VAL))->toString(),
                               VAR.setByUser);
    }

    return "invalid type (internal error)";
}

static std::string decorationRequest(eHyprCtlOutputFormat format, std::string request) {
    CVarList   vars(request, 0, ' ');
    const auto PWINDOW = Desktop::viewState()->query().selector(vars[1]).runWindow();

    if (!PWINDOW)
        return "none";

    std::string result = "";
    if (format == eHyprCtlOutputFormat::FORMAT_JSON) {
        result += "[";
        for (auto const& wd : PWINDOW->presentation().decorations()) {
            result += std::format("{{\n\"decorationName\": \"{}\",\n\"priority\": {}\n}},", wd->getDisplayName(), wd->getPositioningInfo().priority);
        }

        trimTrailingComma(result);
        result += "]";
    } else {
        result = +"Decoration\tPriority\n";
        for (auto const& wd : PWINDOW->presentation().decorations()) {
            result += std::format("{}\t{}\n", wd->getDisplayName(), wd->getPositioningInfo().priority);
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
        for (auto const& m : State::monitorState()->allMonitors()) {
            if (m->m_name == vars[3])
                return "Name already taken";
        }
    }

    if (MODE == "create" || MODE == "add") {
        if (State::monitorState()->query().name(vars[3]).run())
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
        const auto PMONITOR = State::monitorState()->query().name(vars[2]).run();

        if (!PMONITOR)
            return "output not found";

        if (!PMONITOR->m_createdByUser)
            return "cannot remove a real display. Use the monitor keyword.";

        PMONITOR->m_output->destroy();
    }

    return "ok";
}

static SResponse dispatchPlugin(const SRequest& request) {
    CVarList vars(request.command, 0, ' ');

    if (vars.size() < 2)
        return "not enough args";

    const auto OPERATION = vars[1];

    if (OPERATION == "load") {
        if (vars.size() < 3)
            return "not enough args";

        const std::string PATH    = vars[2];
        auto              promise = CPromise<std::string>::make([PATH, pid = request.pid](SP<CPromiseResolver<std::string>> resolver) {
            g_pPluginSystem->loadPlugin(PATH, SPECIAL_PID_TYPE_NONE, pid)->then([resolver](SP<CPromiseResult<CPlugin*>> result) {
                if (result->hasError()) {
                    resolver->reject(result->error());
                    return;
                }

                resolver->resolve("ok");
            });
        });

        return promise;
    } else if (OPERATION == "unload") {
        if (vars.size() < 3)
            return "not enough args";

        const std::string PATH   = vars[2];
        const auto        PLUGIN = g_pPluginSystem->getPluginByPath(PATH);

        if (!PLUGIN)
            return "plugin not loaded";

        g_pPluginSystem->unloadPlugin(PLUGIN);
    } else if (OPERATION == "list") {
        const auto  PLUGINS = g_pPluginSystem->getAllPlugins();
        std::string result  = "";

        if (request.format == eHyprCtlOutputFormat::FORMAT_JSON) {
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

    const auto COLOR_RESULT = Config::ParserUtils::parseColor(vars[3]);
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

    Notification::overlay()->addNotification(MESSAGE, color, time, sc<eIcons>(icon), fontsize);

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

    Notification::overlay()->dismissNotifications(amount);

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
    return Config::Values::getAsJson();
}

static std::string submapRequest(eHyprCtlOutputFormat format, std::string request) {
    std::string submap{Keybinds::mgr()->currentSubmap()};
    if (submap.empty())
        submap = "default";

    return format == FORMAT_JSON ? std::format("\"{}\"\n", escapeJSONStrings(submap)) : std::format("{}\n", submap);
}

static std::string reloadShaders(eHyprCtlOutputFormat format, std::string request) {
    CVarList vars(request, 0, ' ');

    if (vars.size() > 2)
        return "too many args";

    if (g_pHyprOpenGL && g_pHyprRenderer->reloadShaders(vars.size() == 2 ? vars[1] : ""))
        return format == FORMAT_JSON ? "{\"ok\": true}" : "ok";
    else
        return format == FORMAT_JSON ? "{\"ok\": false}" : "error";
}

template <typename F>
static SCommand legacyCommand(std::string name, eCommandMatch match, F handler) {
    return SCommand{
        .name    = std::move(name),
        .match   = match,
        .handler = [handler](const SRequest& request) { return SResponse{handler(request.format, request.command)}; },
    };
}

void IPC::Socket1::registerBuiltinCommands(CSocket1& socket) {
    socket.registerCommand(legacyCommand("workspaces", COMMAND_MATCH_EXACT, workspacesRequest));
    socket.registerCommand(legacyCommand("workspacerules", COMMAND_MATCH_EXACT, workspaceRulesRequest));
    socket.registerCommand(legacyCommand("activeworkspace", COMMAND_MATCH_EXACT, activeWorkspaceRequest));
    socket.registerCommand(SCommand{.name = "clients", .match = COMMAND_MATCH_EXACT, .handler = [](const SRequest& request) { return clientsRequest(request); }});
    socket.registerCommand(legacyCommand("kill", COMMAND_MATCH_EXACT, killRequest));
    socket.registerCommand(legacyCommand("activewindow", COMMAND_MATCH_EXACT, activeWindowRequest));
    socket.registerCommand(legacyCommand("layers", COMMAND_MATCH_EXACT, layersRequest));
    socket.registerCommand(legacyCommand("version", COMMAND_MATCH_EXACT, versionRequest));
    socket.registerCommand(legacyCommand("devices", COMMAND_MATCH_EXACT, devicesRequest));
    socket.registerCommand(legacyCommand("splash", COMMAND_MATCH_EXACT, splashRequest));
    socket.registerCommand(legacyCommand("cursorpos", COMMAND_MATCH_EXACT, cursorPosRequest));
    socket.registerCommand(legacyCommand("binds", COMMAND_MATCH_EXACT, bindsRequest));
    socket.registerCommand(legacyCommand("globalshortcuts", COMMAND_MATCH_EXACT, globalShortcutsRequest));
    socket.registerCommand(SCommand{.name = "systeminfo", .match = COMMAND_MATCH_EXACT, .handler = [](const SRequest& request) { return systemInfoRequest(request); }});
    socket.registerCommand(legacyCommand("animations", COMMAND_MATCH_EXACT, animationsRequest));
    socket.registerCommand(SCommand{
        .name    = "rollinglog",
        .match   = COMMAND_MATCH_EXACT,
        .handler = [](const SRequest& request) { return SResponse{rollinglogRequest(request.format, request.command), request.follow ? REPLY_MODE_FOLLOW : REPLY_MODE_CLOSE}; },
    });
    socket.registerCommand(legacyCommand("configerrors", COMMAND_MATCH_EXACT, configErrorsRequest));
    socket.registerCommand(legacyCommand("locked", COMMAND_MATCH_EXACT, getIsLocked));
    socket.registerCommand(legacyCommand("descriptions", COMMAND_MATCH_EXACT, getDescriptions));
    socket.registerCommand(legacyCommand("submap", COMMAND_MATCH_EXACT, submapRequest));
    socket.registerCommand(legacyCommand("status", COMMAND_MATCH_EXACT, statusRequest));
    socket.registerCommand(legacyCommand("deprecated-config", COMMAND_MATCH_EXACT, deprecatedConfigRequest));

    socket.registerCommand(legacyCommand("reloadshaders", COMMAND_MATCH_PREFIX, reloadShaders));
    socket.registerCommand(legacyCommand("monitors", COMMAND_MATCH_PREFIX, monitorsRequest));
    socket.registerCommand(legacyCommand("reload", COMMAND_MATCH_PREFIX, reloadRequest));
    socket.registerCommand(SCommand{.name = "plugin", .match = COMMAND_MATCH_PREFIX, .handler = dispatchPlugin});
    socket.registerCommand(legacyCommand("notify", COMMAND_MATCH_PREFIX, dispatchNotify));
    socket.registerCommand(legacyCommand("dismissnotify", COMMAND_MATCH_PREFIX, dispatchDismissNotify));
    socket.registerCommand(legacyCommand("getprop", COMMAND_MATCH_PREFIX, dispatchGetProp));
    socket.registerCommand(legacyCommand("seterror", COMMAND_MATCH_PREFIX, dispatchSeterror));
    socket.registerCommand(legacyCommand("switchxkblayout", COMMAND_MATCH_PREFIX, switchXKBLayoutRequest));
    socket.registerCommand(legacyCommand("output", COMMAND_MATCH_PREFIX, dispatchOutput));
    socket.registerCommand(legacyCommand("dispatch", COMMAND_MATCH_PREFIX, dispatchRequest));
    socket.registerCommand(legacyCommand("setcursor", COMMAND_MATCH_PREFIX, dispatchSetCursor));
    socket.registerCommand(legacyCommand("getoption", COMMAND_MATCH_PREFIX, dispatchGetOption));
    socket.registerCommand(legacyCommand("decorations", COMMAND_MATCH_PREFIX, decorationRequest));
    socket.registerCommand(legacyCommand("eval", COMMAND_MATCH_PREFIX, evalRequest));
    socket.registerCommand(legacyCommand("repl", COMMAND_MATCH_PREFIX, evalRequest));
}

void IPC::Socket1::refreshState() {
    Config::monitorRuleMgr()->scheduleReload();
    Layout::Supplementary::algoMatcher()->updateWorkspaceLayouts();

    g_pInputManager->setKeyboardLayout();
    g_pInputManager->setPointerConfigs();
    g_pInputManager->setTouchDeviceConfigs();
    g_pInputManager->setTabletConfigs();

    g_pHyprRenderer->m_reloadScreenShader = true;

    for (const auto& monitor : State::monitorState()->monitors()) {
        if (monitor)
            monitor->m_blurFBDirty = true;
    }

    for (const auto& window : Desktop::windowState()->windows()) {
        if (!window->mapped() || !window->m_workspace || !window->m_workspace->isVisible())
            continue;

        Desktop::Rule::ruleEngine()->updateAllRules();
    }

    for (const auto& workspace : State::workspaceState()->workspaces()) {
        if (!workspace)
            continue;

        workspace->updateWindows();
        workspace->updateWindowData();
        workspace->updateWindowDecos();
    }

    for (const auto& monitor : State::monitorState()->monitors())
        g_pHyprRenderer->damageMonitor(monitor);
}
