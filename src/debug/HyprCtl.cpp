#include "HyprCtl.hpp"

#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/un.h>
#include <unistd.h>

#include <sstream>
#include <string>
#include <typeindex>

#include "../config/ConfigDataValues.hpp"
#include "../config/ConfigValue.hpp"
#include "../managers/CursorManager.hpp"
#include "../hyprerror/HyprError.hpp"
#include "../devices/IPointer.hpp"
#include "../devices/IKeyboard.hpp"
#include "../devices/ITouch.hpp"
#include "../devices/Tablet.hpp"

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

static std::string availableModesForOutput(CMonitor* pMonitor, eHyprCtlOutputFormat format) {
    std::string result;

    if (!wl_list_empty(&pMonitor->output->modes)) {
        wlr_output_mode* mode;

        wl_list_for_each(mode, &pMonitor->output->modes, link) {

            if (format == FORMAT_NORMAL)
                result += std::format("{}x{}@{:.2f}Hz ", mode->width, mode->height, mode->refresh / 1000.0);
            else
                result += std::format("\"{}x{}@{:.2f}Hz\",", mode->width, mode->height, mode->refresh / 1000.0);
        }

        result.pop_back();
    }

    return result;
}

std::string monitorsRequest(eHyprCtlOutputFormat format, std::string request) {
    CVarList vars(request, 0, ' ');
    auto     allMonitors = false;

    if (vars.size() > 2)
        return "too many args";

    if (vars.size() == 2 && vars[1] == "all")
        allMonitors = true;

    std::string result = "";
    if (format == eHyprCtlOutputFormat::FORMAT_JSON) {
        result += "[";

        for (auto& m : allMonitors ? g_pCompositor->m_vRealMonitors : g_pCompositor->m_vMonitors) {
            if (!m->output || m->ID == -1ull)
                continue;

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
    "activelyTearing": {},
    "disabled": {},
    "currentFormat": "{}",
    "availableModes": [{}]
}},)#",
                m->ID, escapeJSONStrings(m->szName), escapeJSONStrings(m->szShortDescription), escapeJSONStrings(m->output->make ? m->output->make : ""),
                escapeJSONStrings(m->output->model ? m->output->model : ""), escapeJSONStrings(m->output->serial ? m->output->serial : ""), (int)m->vecPixelSize.x,
                (int)m->vecPixelSize.y, m->refreshRate, (int)m->vecPosition.x, (int)m->vecPosition.y, m->activeWorkspaceID(),
                (!m->activeWorkspace ? "" : escapeJSONStrings(m->activeWorkspace->m_szName)), m->activeSpecialWorkspaceID(),
                escapeJSONStrings(m->activeSpecialWorkspace ? m->activeSpecialWorkspace->m_szName : ""), (int)m->vecReservedTopLeft.x, (int)m->vecReservedTopLeft.y,
                (int)m->vecReservedBottomRight.x, (int)m->vecReservedBottomRight.y, m->scale, (int)m->transform, (m == g_pCompositor->m_pLastMonitor ? "true" : "false"),
                (m->dpmsStatus ? "true" : "false"), (m->output->adaptive_sync_status == WLR_OUTPUT_ADAPTIVE_SYNC_ENABLED ? "true" : "false"),
                (m->tearingState.activelyTearing ? "true" : "false"), (m->m_bEnabled ? "false" : "true"), formatToString(m->drmFormat), availableModesForOutput(m.get(), format));
        }

        trimTrailingComma(result);

        result += "]";
    } else {
        for (auto& m : allMonitors ? g_pCompositor->m_vRealMonitors : g_pCompositor->m_vMonitors) {
            if (!m->output || m->ID == -1ull)
                continue;

            result += std::format(
                "Monitor {} (ID {}):\n\t{}x{}@{:.5f} at {}x{}\n\tdescription: {}\n\tmake: {}\n\tmodel: {}\n\tserial: {}\n\tactive workspace: {} ({})\n\tspecial "
                "workspace: {} ({})\n\treserved: {} "
                "{} {} {}\n\tscale: {:.2f}\n\ttransform: "
                "{}\n\tfocused: {}\n\tdpmsStatus: {}\n\tvrr: {}\n\tactivelyTearing: {}\n\tdisabled: {}\n\tcurrentFormat: {}\n\tavailableModes: {}\n\n",
                m->szName, m->ID, (int)m->vecPixelSize.x, (int)m->vecPixelSize.y, m->refreshRate, (int)m->vecPosition.x, (int)m->vecPosition.y, m->szShortDescription,
                (m->output->make ? m->output->make : ""), (m->output->model ? m->output->model : ""), (m->output->serial ? m->output->serial : ""), m->activeWorkspaceID(),
                (!m->activeWorkspace ? "" : m->activeWorkspace->m_szName), m->activeSpecialWorkspaceID(), (m->activeSpecialWorkspace ? m->activeSpecialWorkspace->m_szName : ""),
                (int)m->vecReservedTopLeft.x, (int)m->vecReservedTopLeft.y, (int)m->vecReservedBottomRight.x, (int)m->vecReservedBottomRight.y, m->scale, (int)m->transform,
                (m == g_pCompositor->m_pLastMonitor ? "yes" : "no"), (int)m->dpmsStatus, (int)(m->output->adaptive_sync_status == WLR_OUTPUT_ADAPTIVE_SYNC_ENABLED),
                m->tearingState.activelyTearing, !m->m_bEnabled, formatToString(m->drmFormat), availableModesForOutput(m.get(), format));
        }
    }

    return result;
}

static std::string getGroupedData(PHLWINDOW w, eHyprCtlOutputFormat format) {
    const bool isJson = format == eHyprCtlOutputFormat::FORMAT_JSON;
    if (w->m_sGroupData.pNextWindow.expired())
        return isJson ? "" : "0";

    std::ostringstream result;

    PHLWINDOW          head = w->getGroupHead();
    PHLWINDOW          curr = head;
    while (true) {
        if (isJson)
            result << std::format("\"0x{:x}\"", (uintptr_t)curr.get());
        else
            result << std::format("{:x}", (uintptr_t)curr.get());
        curr = curr->m_sGroupData.pNextWindow.lock();
        // We've wrapped around to the start, break out without trailing comma
        if (curr == head)
            break;
        result << (isJson ? ", " : ",");
    }

    return result.str();
}

static std::string getWindowData(PHLWINDOW w, eHyprCtlOutputFormat format) {
    auto getFocusHistoryID = [](PHLWINDOW wnd) -> int {
        for (size_t i = 0; i < g_pCompositor->m_vWindowFocusHistory.size(); ++i) {
            if (g_pCompositor->m_vWindowFocusHistory[i].lock() == wnd)
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
    "monitor": {},
    "class": "{}",
    "title": "{}",
    "initialClass": "{}",
    "initialTitle": "{}",
    "pid": {},
    "xwayland": {},
    "pinned": {},
    "fullscreen": {},
    "fullscreenMode": {},
    "fakeFullscreen": {},
    "grouped": [{}],
    "swallowing": "0x{:x}",
    "focusHistoryID": {}
}},)#",
            (uintptr_t)w.get(), (w->m_bIsMapped ? "true" : "false"), (w->isHidden() ? "true" : "false"), (int)w->m_vRealPosition.goal().x, (int)w->m_vRealPosition.goal().y,
            (int)w->m_vRealSize.goal().x, (int)w->m_vRealSize.goal().y, w->m_pWorkspace ? w->workspaceID() : WORKSPACE_INVALID,
            escapeJSONStrings(!w->m_pWorkspace ? "" : w->m_pWorkspace->m_szName), ((int)w->m_bIsFloating == 1 ? "true" : "false"), (int64_t)w->m_iMonitorID,
            escapeJSONStrings(g_pXWaylandManager->getAppIDClass(w)), escapeJSONStrings(g_pXWaylandManager->getTitle(w)), escapeJSONStrings(w->m_szInitialClass),
            escapeJSONStrings(w->m_szInitialTitle), w->getPID(), ((int)w->m_bIsX11 == 1 ? "true" : "false"), (w->m_bPinned ? "true" : "false"),
            (w->m_bIsFullscreen ? "true" : "false"), (w->m_bIsFullscreen ? (w->m_pWorkspace ? (int)w->m_pWorkspace->m_efFullscreenMode : 0) : 0),
            w->m_bFakeFullscreenState ? "true" : "false", getGroupedData(w, format), (uintptr_t)w->m_pSwallowed.lock().get(), getFocusHistoryID(w));
    } else {
        return std::format("Window {:x} -> {}:\n\tmapped: {}\n\thidden: {}\n\tat: {},{}\n\tsize: {},{}\n\tworkspace: {} ({})\n\tfloating: {}\n\tmonitor: {}\n\tclass: {}\n\ttitle: "
                           "{}\n\tinitialClass: {}\n\tinitialTitle: {}\n\tpid: "
                           "{}\n\txwayland: {}\n\tpinned: "
                           "{}\n\tfullscreen: {}\n\tfullscreenmode: {}\n\tfakefullscreen: {}\n\tgrouped: {}\n\tswallowing: {:x}\n\tfocusHistoryID: {}\n\n",
                           (uintptr_t)w.get(), w->m_szTitle, (int)w->m_bIsMapped, (int)w->isHidden(), (int)w->m_vRealPosition.goal().x, (int)w->m_vRealPosition.goal().y,
                           (int)w->m_vRealSize.goal().x, (int)w->m_vRealSize.goal().y, w->m_pWorkspace ? w->workspaceID() : WORKSPACE_INVALID,
                           (!w->m_pWorkspace ? "" : w->m_pWorkspace->m_szName), (int)w->m_bIsFloating, (int64_t)w->m_iMonitorID, g_pXWaylandManager->getAppIDClass(w),
                           g_pXWaylandManager->getTitle(w), w->m_szInitialClass, w->m_szInitialTitle, w->getPID(), (int)w->m_bIsX11, (int)w->m_bPinned, (int)w->m_bIsFullscreen,
                           (w->m_bIsFullscreen ? (w->m_pWorkspace ? w->m_pWorkspace->m_efFullscreenMode : 0) : 0), (int)w->m_bFakeFullscreenState, getGroupedData(w, format),
                           (uintptr_t)w->m_pSwallowed.lock().get(), getFocusHistoryID(w));
    }
}

std::string clientsRequest(eHyprCtlOutputFormat format, std::string request) {
    std::string result = "";
    if (format == eHyprCtlOutputFormat::FORMAT_JSON) {
        result += "[";

        for (auto& w : g_pCompositor->m_vWindows) {
            if (!w->m_bIsMapped && !g_pHyprCtl->m_sCurrentRequestParams.all)
                continue;

            result += getWindowData(w, format);
        }

        trimTrailingComma(result);

        result += "]";
    } else {
        for (auto& w : g_pCompositor->m_vWindows) {
            if (!w->m_bIsMapped && !g_pHyprCtl->m_sCurrentRequestParams.all)
                continue;

            result += getWindowData(w, format);
        }
    }
    return result;
}

static std::string getWorkspaceData(PHLWORKSPACE w, eHyprCtlOutputFormat format) {
    const auto PLASTW   = w->getLastFocusedWindow();
    const auto PMONITOR = g_pCompositor->getMonitorFromID(w->m_iMonitorID);
    if (format == eHyprCtlOutputFormat::FORMAT_JSON) {
        return std::format(R"#({{
    "id": {},
    "name": "{}",
    "monitor": "{}",
    "monitorID": {},
    "windows": {},
    "hasfullscreen": {},
    "lastwindow": "0x{:x}",
    "lastwindowtitle": "{}"
}})#",
                           w->m_iID, escapeJSONStrings(w->m_szName), escapeJSONStrings(PMONITOR ? PMONITOR->szName : "?"),
                           escapeJSONStrings(PMONITOR ? std::to_string(PMONITOR->ID) : "null"), g_pCompositor->getWindowsOnWorkspace(w->m_iID),
                           ((int)w->m_bHasFullscreenWindow == 1 ? "true" : "false"), (uintptr_t)PLASTW.get(), PLASTW ? escapeJSONStrings(PLASTW->m_szTitle) : "");
    } else {
        return std::format("workspace ID {} ({}) on monitor {}:\n\tmonitorID: {}\n\twindows: {}\n\thasfullscreen: {}\n\tlastwindow: 0x{:x}\n\tlastwindowtitle: {}\n\n", w->m_iID,
                           w->m_szName, PMONITOR ? PMONITOR->szName : "?", PMONITOR ? std::to_string(PMONITOR->ID) : "null", g_pCompositor->getWindowsOnWorkspace(w->m_iID),
                           (int)w->m_bHasFullscreenWindow, (uintptr_t)PLASTW.get(), PLASTW ? PLASTW->m_szTitle : "");
    }
}

static std::string getWorkspaceRuleData(const SWorkspaceRule& r, eHyprCtlOutputFormat format) {
    const auto boolToString = [](const bool b) -> std::string { return b ? "true" : "false"; };
    if (format == eHyprCtlOutputFormat::FORMAT_JSON) {
        const std::string monitor    = r.monitor.empty() ? "" : std::format(",\n    \"monitor\": \"{}\"", escapeJSONStrings(r.monitor));
        const std::string default_   = (bool)(r.isDefault) ? std::format(",\n    \"default\": {}", boolToString(r.isDefault)) : "";
        const std::string persistent = (bool)(r.isPersistent) ? std::format(",\n    \"persistent\": {}", boolToString(r.isPersistent)) : "";
        const std::string gapsIn     = (bool)(r.gapsIn) ?
                std::format(",\n    \"gapsIn\": [{}, {}, {}, {}]", r.gapsIn.value().top, r.gapsIn.value().right, r.gapsIn.value().bottom, r.gapsIn.value().left) :
                "";
        const std::string gapsOut    = (bool)(r.gapsOut) ?
               std::format(",\n    \"gapsOut\": [{}, {}, {}, {}]", r.gapsOut.value().top, r.gapsOut.value().right, r.gapsOut.value().bottom, r.gapsOut.value().left) :
               "";
        const std::string borderSize = (bool)(r.borderSize) ? std::format(",\n    \"borderSize\": {}", r.borderSize.value()) : "";
        const std::string border     = (bool)(r.border) ? std::format(",\n    \"border\": {}", boolToString(r.border.value())) : "";
        const std::string rounding   = (bool)(r.rounding) ? std::format(",\n    \"rounding\": {}", boolToString(r.rounding.value())) : "";
        const std::string decorate   = (bool)(r.decorate) ? std::format(",\n    \"decorate\": {}", boolToString(r.decorate.value())) : "";
        const std::string shadow     = (bool)(r.shadow) ? std::format(",\n    \"shadow\": {}", boolToString(r.shadow.value())) : "";

        std::string       result = std::format(R"#({{
    "workspaceString": "{}"{}{}{}{}{}{}{}{}
}})#",
                                               escapeJSONStrings(r.workspaceString), monitor, default_, persistent, gapsIn, gapsOut, borderSize, border, rounding, decorate, shadow);

        return result;
    } else {
        const std::string monitor    = std::format("\tmonitor: {}\n", r.monitor.empty() ? "<unset>" : escapeJSONStrings(r.monitor));
        const std::string default_   = std::format("\tdefault: {}\n", (bool)(r.isDefault) ? boolToString(r.isDefault) : "<unset>");
        const std::string persistent = std::format("\tpersistent: {}\n", (bool)(r.isPersistent) ? boolToString(r.isPersistent) : "<unset>");
        const std::string gapsIn     = (bool)(r.gapsIn) ? std::format("\tgapsIn: {} {} {} {}\n", std::to_string(r.gapsIn.value().top), std::to_string(r.gapsIn.value().right),
                                                                      std::to_string(r.gapsIn.value().bottom), std::to_string(r.gapsIn.value().left)) :
                                                          std::format("\tgapsIn: <unset>\n");
        const std::string gapsOut    = (bool)(r.gapsOut) ? std::format("\tgapsOut: {} {} {} {}\n", std::to_string(r.gapsOut.value().top), std::to_string(r.gapsOut.value().right),
                                                                       std::to_string(r.gapsOut.value().bottom), std::to_string(r.gapsOut.value().left)) :
                                                           std::format("\tgapsOut: <unset>\n");
        const std::string borderSize = std::format("\tborderSize: {}\n", (bool)(r.borderSize) ? std::to_string(r.borderSize.value()) : "<unset>");
        const std::string border     = std::format("\tborder: {}\n", (bool)(r.border) ? boolToString(r.border.value()) : "<unset>");
        const std::string rounding   = std::format("\trounding: {}\n", (bool)(r.rounding) ? boolToString(r.rounding.value()) : "<unset>");
        const std::string decorate   = std::format("\tdecorate: {}\n", (bool)(r.decorate) ? boolToString(r.decorate.value()) : "<unset>");
        const std::string shadow     = std::format("\tshadow: {}\n", (bool)(r.shadow) ? boolToString(r.shadow.value()) : "<unset>");

        std::string       result = std::format("Workspace rule {}:\n{}{}{}{}{}{}{}{}{}{}\n", escapeJSONStrings(r.workspaceString), monitor, default_, persistent, gapsIn, gapsOut,
                                               borderSize, border, rounding, decorate, shadow);

        return result;
    }
}
std::string activeWorkspaceRequest(eHyprCtlOutputFormat format, std::string request) {
    if (!g_pCompositor->m_pLastMonitor)
        return "unsafe state";

    std::string result = "";
    auto        w      = g_pCompositor->m_pLastMonitor->activeWorkspace;

    if (!valid(w))
        return "internal error";

    return getWorkspaceData(w, format);
}

std::string workspacesRequest(eHyprCtlOutputFormat format, std::string request) {
    std::string result = "";

    if (format == eHyprCtlOutputFormat::FORMAT_JSON) {
        result += "[";
        for (auto& w : g_pCompositor->m_vWorkspaces) {
            result += getWorkspaceData(w, format);
            result += ",";
        }

        trimTrailingComma(result);
        result += "]";
    } else {
        for (auto& w : g_pCompositor->m_vWorkspaces) {
            result += getWorkspaceData(w, format);
        }
    }

    return result;
}

std::string workspaceRulesRequest(eHyprCtlOutputFormat format, std::string request) {
    std::string result = "";
    if (format == eHyprCtlOutputFormat::FORMAT_JSON) {
        result += "[";
        for (auto& r : g_pConfigManager->getAllWorkspaceRules()) {
            result += getWorkspaceRuleData(r, format);
            result += ",";
        }

        trimTrailingComma(result);
        result += "]";
    } else {
        for (auto& r : g_pConfigManager->getAllWorkspaceRules()) {
            result += getWorkspaceRuleData(r, format);
        }
    }

    return result;
}

std::string activeWindowRequest(eHyprCtlOutputFormat format, std::string request) {
    const auto PWINDOW = g_pCompositor->m_pLastWindow.lock();

    if (!validMapped(PWINDOW))
        return format == eHyprCtlOutputFormat::FORMAT_JSON ? "{}" : "Invalid";

    auto result = getWindowData(PWINDOW, format);

    if (format == eHyprCtlOutputFormat::FORMAT_JSON)
        result.pop_back();

    return result;
}

std::string layersRequest(eHyprCtlOutputFormat format, std::string request) {
    std::string result = "";

    if (format == eHyprCtlOutputFormat::FORMAT_JSON) {
        result += "{\n";

        for (auto& mon : g_pCompositor->m_vMonitors) {
            result += std::format(
                R"#("{}": {{
    "levels": {{
)#",
                escapeJSONStrings(mon->szName));

            int layerLevel = 0;
            for (auto& level : mon->m_aLayerSurfaceLayers) {
                result += std::format(
                    R"#(
        "{}": [
)#",
                    layerLevel);
                for (auto& layer : level) {
                    result += std::format(
                        R"#(                {{
                    "address": "0x{:x}",
                    "x": {},
                    "y": {},
                    "w": {},
                    "h": {},
                    "namespace": "{}"
                }},)#",
                        (uintptr_t)layer.get(), layer->geometry.x, layer->geometry.y, layer->geometry.width, layer->geometry.height, escapeJSONStrings(layer->szNamespace));
                }

                trimTrailingComma(result);

                if (level.size() > 0)
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
        for (auto& mon : g_pCompositor->m_vMonitors) {
            result += std::format("Monitor {}:\n", mon->szName);
            int                                     layerLevel = 0;
            static const std::array<std::string, 4> levelNames = {"background", "bottom", "top", "overlay"};
            for (auto& level : mon->m_aLayerSurfaceLayers) {
                result += std::format("\tLayer level {} ({}):\n", layerLevel, levelNames[layerLevel]);

                for (auto& layer : level) {
                    result += std::format("\t\tLayer {:x}: xywh: {} {} {} {}, namespace: {}\n", (uintptr_t)layer.get(), layer->geometry.x, layer->geometry.y, layer->geometry.width,
                                          layer->geometry.height, layer->szNamespace);
                }

                layerLevel++;
            }
            result += "\n\n";
        }
    }

    return result;
}

std::string layoutsRequest(eHyprCtlOutputFormat format, std::string request) {
    std::string result = "";
    if (format == eHyprCtlOutputFormat::FORMAT_JSON) {
        result += "[";

        for (auto& m : g_pLayoutManager->getAllLayoutNames()) {
            result += std::format(
                R"#(
    "{}",)#",
                m);
        }
        trimTrailingComma(result);

        result += "\n]\n";
    } else {
        for (auto& m : g_pLayoutManager->getAllLayoutNames()) {
            result += std::format("{}\n", m);
        }
    }
    return result;
}

std::string configErrorsRequest(eHyprCtlOutputFormat format, std::string request) {
    std::string result     = "";
    std::string currErrors = g_pConfigManager->getErrors();
    CVarList    errLines(currErrors, 0, '\n');
    if (format == eHyprCtlOutputFormat::FORMAT_JSON) {
        result += "[";
        for (auto line : errLines) {
            result += std::format(
                R"#(
	"{}",)#",

                escapeJSONStrings(line));
        }
        trimTrailingComma(result);
        result += "\n]\n";
    } else {
        for (auto line : errLines) {
            result += std::format("{}\n", line);
        }
    }
    return result;
}

std::string devicesRequest(eHyprCtlOutputFormat format, std::string request) {
    std::string result = "";

    if (format == eHyprCtlOutputFormat::FORMAT_JSON) {
        result += "{\n";
        result += "\"mice\": [\n";

        for (auto& m : g_pInputManager->m_vPointers) {
            result += std::format(
                R"#(    {{
        "address": "0x{:x}",
        "name": "{}",
        "defaultSpeed": {:.5f}
    }},)#",
                (uintptr_t)m.get(), escapeJSONStrings(m->hlName),
                wlr_input_device_is_libinput(&m->wlr()->base) ? libinput_device_config_accel_get_default_speed((libinput_device*)wlr_libinput_get_device_handle(&m->wlr()->base)) :
                                                                0.f);
        }

        trimTrailingComma(result);
        result += "\n],\n";

        result += "\"keyboards\": [\n";
        for (auto& k : g_pInputManager->m_vKeyboards) {
            const auto KM = k->getActiveLayout();
            result += std::format(
                R"#(    {{
        "address": "0x{:x}",
        "name": "{}",
        "rules": "{}",
        "model": "{}",
        "layout": "{}",
        "variant": "{}",
        "options": "{}",
        "active_keymap": "{}",
        "main": {}
    }},)#",
                (uintptr_t)k.get(), escapeJSONStrings(k->hlName), escapeJSONStrings(k->currentRules.rules), escapeJSONStrings(k->currentRules.model),
                escapeJSONStrings(k->currentRules.layout), escapeJSONStrings(k->currentRules.variant), escapeJSONStrings(k->currentRules.options), escapeJSONStrings(KM),
                (k->active ? "true" : "false"));
        }

        trimTrailingComma(result);
        result += "\n],\n";

        result += "\"tablets\": [\n";

        for (auto& d : g_pInputManager->m_vTabletPads) {
            result += std::format(
                R"#(    {{
        "address": "0x{:x}",
        "type": "tabletPad",
        "belongsTo": {{
            "address": "0x{:x}",
            "name": "{}"
        }}
    }},)#",
                (uintptr_t)d.get(), (uintptr_t)d->parent.get(), escapeJSONStrings(d->parent ? d->parent->hlName : ""));
        }

        for (auto& d : g_pInputManager->m_vTablets) {
            result += std::format(
                R"#(    {{
        "address": "0x{:x}",
        "name": "{}"
    }},)#",
                (uintptr_t)d.get(), escapeJSONStrings(d->hlName));
        }

        for (auto& d : g_pInputManager->m_vTabletTools) {
            result += std::format(
                R"#(    {{
        "address": "0x{:x}",
        "type": "tabletTool",
        "belongsTo": "0x{:x}"
    }},)#",
                (uintptr_t)d.get(), d->wlr() ? (uintptr_t)d->wlr()->data : 0);
        }

        trimTrailingComma(result);
        result += "\n],\n";

        result += "\"touch\": [\n";

        for (auto& d : g_pInputManager->m_vTouches) {
            result += std::format(
                R"#(    {{
        "address": "0x{:x}",
        "name": "{}"
    }},)#",
                (uintptr_t)d.get(), escapeJSONStrings(d->hlName));
        }

        trimTrailingComma(result);
        result += "\n],\n";

        result += "\"switches\": [\n";

        for (auto& d : g_pInputManager->m_lSwitches) {
            result += std::format(
                R"#(    {{
        "address": "0x{:x}",
        "name": "{}"
    }},)#",
                (uintptr_t)&d, escapeJSONStrings(d.pWlrDevice ? d.pWlrDevice->name : ""));
        }

        trimTrailingComma(result);
        result += "\n]\n";

        result += "}\n";

    } else {
        result += "mice:\n";

        for (auto& m : g_pInputManager->m_vPointers) {
            result += std::format("\tMouse at {:x}:\n\t\t{}\n\t\t\tdefault speed: {:.5f}\n", (uintptr_t)m.get(), m->hlName,
                                  (wlr_input_device_is_libinput(&m->wlr()->base) ?
                                       libinput_device_config_accel_get_default_speed((libinput_device*)wlr_libinput_get_device_handle(&m->wlr()->base)) :
                                       0.f));
        }

        result += "\n\nKeyboards:\n";

        for (auto& k : g_pInputManager->m_vKeyboards) {
            const auto KM = k->getActiveLayout();
            result += std::format("\tKeyboard at {:x}:\n\t\t{}\n\t\t\trules: r \"{}\", m \"{}\", l \"{}\", v \"{}\", o \"{}\"\n\t\t\tactive keymap: {}\n\t\t\tmain: {}\n",
                                  (uintptr_t)k.get(), k->hlName, k->currentRules.rules, k->currentRules.model, k->currentRules.layout, k->currentRules.variant,
                                  k->currentRules.options, KM, (k->active ? "yes" : "no"));
        }

        result += "\n\nTablets:\n";

        for (auto& d : g_pInputManager->m_vTabletPads) {
            result += std::format("\tTablet Pad at {:x} (belongs to {:x} -> {})\n", (uintptr_t)d.get(), (uintptr_t)d->parent.get(), d->parent ? d->parent->hlName : "");
        }

        for (auto& d : g_pInputManager->m_vTablets) {
            result += std::format("\tTablet at {:x}:\n\t\t{}\n\t\t\tsize: {}x{}mm\n", (uintptr_t)d.get(), d->hlName, d->wlr()->width_mm, d->wlr()->height_mm);
        }

        for (auto& d : g_pInputManager->m_vTabletTools) {
            result += std::format("\tTablet Tool at {:x} (belongs to {:x})\n", (uintptr_t)d.get(), d->wlr() ? (uintptr_t)d->wlr()->data : 0);
        }

        result += "\n\nTouch:\n";

        for (auto& d : g_pInputManager->m_vTouches) {
            result += std::format("\tTouch Device at {:x}:\n\t\t{}\n", (uintptr_t)d.get(), d->hlName);
        }

        result += "\n\nSwitches:\n";

        for (auto& d : g_pInputManager->m_lSwitches) {
            result += std::format("\tSwitch Device at {:x}:\n\t\t{}\n", (uintptr_t)&d, d.pWlrDevice ? d.pWlrDevice->name : "");
        }
    }

    return result;
}

std::string animationsRequest(eHyprCtlOutputFormat format, std::string request) {
    std::string ret = "";
    if (format == eHyprCtlOutputFormat::FORMAT_NORMAL) {
        ret += "animations:\n";

        for (auto& ac : g_pConfigManager->getAnimationConfig()) {
            ret += std::format("\n\tname: {}\n\t\toverriden: {}\n\t\tbezier: {}\n\t\tenabled: {}\n\t\tspeed: {:.2f}\n\t\tstyle: {}\n", ac.first, (int)ac.second.overridden,
                               ac.second.internalBezier, ac.second.internalEnabled, ac.second.internalSpeed, ac.second.internalStyle);
        }

        ret += "beziers:\n";

        for (auto& bz : g_pAnimationManager->getAllBeziers()) {
            ret += std::format("\n\tname: {}\n", bz.first);
        }
    } else {
        // json

        ret += "[[";
        for (auto& ac : g_pConfigManager->getAnimationConfig()) {
            ret += std::format(R"#(
{{
    "name": "{}",
    "overridden": {},
    "bezier": "{}",
    "enabled": {},
    "speed": {:.2f},
    "style": "{}"
}},)#",
                               ac.first, ac.second.overridden ? "true" : "false", escapeJSONStrings(ac.second.internalBezier), ac.second.internalEnabled ? "true" : "false",
                               ac.second.internalSpeed, escapeJSONStrings(ac.second.internalStyle));
        }

        ret[ret.length() - 1] = ']';

        ret += ",\n[";

        for (auto& bz : g_pAnimationManager->getAllBeziers()) {
            ret += std::format(R"#(
{{
    "name": "{}"
}},)#",
                               escapeJSONStrings(bz.first));
        }

        trimTrailingComma(ret);

        ret += "]]";
    }

    return ret;
}

std::string rollinglogRequest(eHyprCtlOutputFormat format, std::string request) {
    std::string result = "";

    if (format == eHyprCtlOutputFormat::FORMAT_JSON) {
        result += "[\n\"log\":\"";
        result += escapeJSONStrings(Debug::rollingLog);
        result += "\"]";
    } else {
        result = Debug::rollingLog;
    }

    return result;
}

std::string globalShortcutsRequest(eHyprCtlOutputFormat format, std::string request) {
    std::string ret       = "";
    const auto  SHORTCUTS = g_pProtocolManager->m_pGlobalShortcutsProtocolManager->getAllShortcuts();
    if (format == eHyprCtlOutputFormat::FORMAT_NORMAL) {
        for (auto& sh : SHORTCUTS)
            ret += std::format("{}:{} -> {}\n", sh.appid, sh.id, sh.description);
    } else {
        ret += "[";
        for (auto& sh : SHORTCUTS) {
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

std::string bindsRequest(eHyprCtlOutputFormat format, std::string request) {
    std::string ret = "";
    if (format == eHyprCtlOutputFormat::FORMAT_NORMAL) {
        for (auto& kb : g_pKeybindManager->m_lKeybinds) {
            ret += "bind";
            if (kb.locked)
                ret += "l";
            if (kb.mouse)
                ret += "m";
            if (kb.release)
                ret += "r";
            if (kb.repeat)
                ret += "e";
            if (kb.nonConsuming)
                ret += "n";

            ret += std::format("\n\tmodmask: {}\n\tsubmap: {}\n\tkey: {}\n\tkeycode: {}\n\tcatchall: {}\n\tdispatcher: {}\n\targ: {}\n\n", kb.modmask, kb.submap, kb.key,
                               kb.keycode, kb.catchAll, kb.handler, kb.arg);
        }
    } else {
        // json
        ret += "[";
        for (auto& kb : g_pKeybindManager->m_lKeybinds) {
            ret += std::format(
                R"#(
{{
    "locked": {},
    "mouse": {},
    "release": {},
    "repeat": {},
    "non_consuming": {},
    "modmask": {},
    "submap": "{}",
    "key": "{}",
    "keycode": {},
    "catch_all": {},
    "dispatcher": "{}",
    "arg": "{}"
}},)#",
                kb.locked ? "true" : "false", kb.mouse ? "true" : "false", kb.release ? "true" : "false", kb.repeat ? "true" : "false", kb.nonConsuming ? "true" : "false",
                kb.modmask, escapeJSONStrings(kb.submap), escapeJSONStrings(kb.key), kb.keycode, kb.catchAll ? "true" : "false", escapeJSONStrings(kb.handler),
                escapeJSONStrings(kb.arg));
        }
        trimTrailingComma(ret);
        ret += "]";
    }

    return ret;
}

std::string versionRequest(eHyprCtlOutputFormat format, std::string request) {

    auto commitMsg = removeBeginEndSpacesTabs(GIT_COMMIT_MESSAGE);
    std::replace(commitMsg.begin(), commitMsg.end(), '#', ' ');

    if (format == eHyprCtlOutputFormat::FORMAT_NORMAL) {
        std::string result = "Hyprland, built from branch " + std::string(GIT_BRANCH) + " at commit " + GIT_COMMIT_HASH + " " + GIT_DIRTY + " (" + commitMsg +
            ").\nDate: " + GIT_COMMIT_DATE + "\nTag: " + GIT_TAG + ", commits: " + GIT_COMMITS + "\n\nflags: (if any)\n";

#ifdef LEGACY_RENDERER
        result += "legacyrenderer\n";
#endif
#ifndef ISDEBUG
        result += "debug\n";
#endif
#ifdef NO_XWAYLAND
        result += "no xwayland\n";
#endif

        return result;
    } else {
        std::string result = std::format(
            R"#({{
    "branch": "{}",
    "commit": "{}",
    "dirty": {},
    "commit_message": "{}",
    "commit_date": "{}",
    "tag": "{}",
    "commits": "{}",
    "flags": [)#",
            GIT_BRANCH, GIT_COMMIT_HASH, (strcmp(GIT_DIRTY, "dirty") == 0 ? "true" : "false"), escapeJSONStrings(commitMsg), GIT_COMMIT_DATE, GIT_TAG, GIT_COMMITS);

#ifdef LEGACY_RENDERER
        result += "\"legacyrenderer\",";
#endif
#ifndef ISDEBUG
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

    result += "\n\nSystem Information:\n";

    struct utsname unameInfo;

    uname(&unameInfo);

    result += "System name: " + std::string{unameInfo.sysname} + "\n";
    result += "Node name: " + std::string{unameInfo.nodename} + "\n";
    result += "Release: " + std::string{unameInfo.release} + "\n";
    result += "Version: " + std::string{unameInfo.version} + "\n";

    result += "\n\n";

#if defined(__DragonFly__) || defined(__FreeBSD__)
    const std::string GPUINFO = execAndGet("pciconf -lv | fgrep -A4 vga");
#else
    const std::string GPUINFO = execAndGet("lspci -vnn | grep VGA");
#endif
    result += "GPU information: \n" + GPUINFO + "\n\n";

    result += "os-release: " + execAndGet("cat /etc/os-release") + "\n\n";

    result += "plugins:\n";
    for (auto& pl : g_pPluginSystem->getAllPlugins()) {
        result += std::format("  {} by {} ver {}\n", pl->name, pl->author, pl->version);
    }

    return result;
}

std::string dispatchRequest(eHyprCtlOutputFormat format, std::string in) {
    // get rid of the dispatch keyword
    in = in.substr(in.find_first_of(' ') + 1);

    const auto DISPATCHSTR = in.substr(0, in.find_first_of(' '));

    auto       DISPATCHARG = std::string();
    if ((int)in.find_first_of(' ') != -1)
        DISPATCHARG = in.substr(in.find_first_of(' ') + 1);

    const auto DISPATCHER = g_pKeybindManager->m_mDispatchers.find(DISPATCHSTR);
    if (DISPATCHER == g_pKeybindManager->m_mDispatchers.end())
        return "Invalid dispatcher";

    DISPATCHER->second(DISPATCHARG);

    Debug::log(LOG, "Hyprctl: dispatcher {} : {}", DISPATCHSTR, DISPATCHARG);

    return "ok";
}

std::string dispatchKeyword(eHyprCtlOutputFormat format, std::string in) {
    // get rid of the keyword keyword
    in = in.substr(in.find_first_of(' ') + 1);

    const auto  COMMAND = in.substr(0, in.find_first_of(' '));

    const auto  VALUE = in.substr(in.find_first_of(' ') + 1);

    std::string retval = g_pConfigManager->parseKeyword(COMMAND, VALUE);

    // if we are executing a dynamic source we have to reload everything, so every if will have a check for source.
    if (COMMAND == "monitor" || COMMAND == "source")
        g_pConfigManager->m_bWantsMonitorReload = true; // for monitor keywords

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
        g_pHyprOpenGL->m_bReloadScreenShader = true;

    if (COMMAND.contains("blur") || COMMAND == "source") {
        for (auto& [m, rd] : g_pHyprOpenGL->m_mMonitorRenderResources) {
            rd.blurFBDirty = true;
        }
    }

    // decorations will probably need a repaint
    if (COMMAND.contains("decoration:") || COMMAND.contains("border") || COMMAND == "workspace" || COMMAND.contains("zoom_factor") || COMMAND == "source") {
        for (auto& m : g_pCompositor->m_vMonitors) {
            g_pHyprRenderer->damageMonitor(m.get());
            g_pLayoutManager->getCurrentLayout()->recalculateMonitor(m->ID);
        }
    }

    Debug::log(LOG, "Hyprctl: keyword {} : {}", COMMAND, VALUE);

    if (retval == "")
        return "ok";

    return retval;
}

std::string reloadRequest(eHyprCtlOutputFormat format, std::string request) {

    const auto REQMODE = request.substr(request.find_last_of(' ') + 1);

    g_pConfigManager->m_bForceReload = true;

    if (REQMODE == "config-only") {
        g_pConfigManager->m_bNoMonitorReload = true;
    }

    g_pConfigManager->tick();

    return "ok";
}

std::string killRequest(eHyprCtlOutputFormat format, std::string request) {
    g_pInputManager->setClickMode(CLICKMODE_KILL);

    return "ok";
}

std::string splashRequest(eHyprCtlOutputFormat format, std::string request) {
    return g_pCompositor->m_szCurrentSplash;
}

std::string cursorPosRequest(eHyprCtlOutputFormat format, std::string request) {
    const auto CURSORPOS = g_pInputManager->getMouseCoordsInternal().floor();

    if (format == eHyprCtlOutputFormat::FORMAT_NORMAL) {
        return std::format("{}, {}", (int)CURSORPOS.x, (int)CURSORPOS.y);
    } else {
        return std::format(R"#(
{{
    "x": {},
    "y": {}
}}
)#",
                           (int)CURSORPOS.x, (int)CURSORPOS.y);
    }

    return "error";
}

std::string dispatchBatch(eHyprCtlOutputFormat format, std::string request) {
    // split by ;

    request             = request.substr(9);
    std::string curitem = "";
    std::string reply   = "";

    auto        nextItem = [&]() {
        auto idx = request.find_first_of(';');

        if (idx != std::string::npos) {
            curitem = request.substr(0, idx);
            request = request.substr(idx + 1);
        } else {
            curitem = request;
            request = "";
        }

        curitem = removeBeginEndSpacesTabs(curitem);
    };

    nextItem();

    while (curitem != "" || request != "") {
        reply += g_pHyprCtl->getReply(curitem);

        nextItem();
    }

    return reply;
}

std::string dispatchSetCursor(eHyprCtlOutputFormat format, std::string request) {
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

    g_pCursorManager->changeTheme(theme, size);

    return "ok";
}

std::string switchXKBLayoutRequest(eHyprCtlOutputFormat format, std::string request) {
    CVarList   vars(request, 0, ' ');

    const auto KB  = vars[1];
    const auto CMD = vars[2];

    // get kb
    const auto PKEYBOARD = std::find_if(g_pInputManager->m_vKeyboards.begin(), g_pInputManager->m_vKeyboards.end(),
                                        [&](const auto& other) { return other->hlName == g_pInputManager->deviceNameToInternalString(KB); });

    if (PKEYBOARD == g_pInputManager->m_vKeyboards.end())
        return "device not found";

    const auto         PWLRKEYBOARD = (*PKEYBOARD)->wlr();
    const auto         LAYOUTS      = xkb_keymap_num_layouts(PWLRKEYBOARD->keymap);
    xkb_layout_index_t activeLayout = 0;
    while (activeLayout < LAYOUTS) {
        if (xkb_state_layout_index_is_active(PWLRKEYBOARD->xkb_state, activeLayout, XKB_STATE_LAYOUT_EFFECTIVE) == 1)
            break;

        activeLayout++;
    }

    if (CMD == "next") {
        wlr_keyboard_notify_modifiers(PWLRKEYBOARD, PWLRKEYBOARD->modifiers.depressed, PWLRKEYBOARD->modifiers.latched, PWLRKEYBOARD->modifiers.locked,
                                      activeLayout > LAYOUTS ? 0 : activeLayout + 1);
    } else if (CMD == "prev") {
        wlr_keyboard_notify_modifiers(PWLRKEYBOARD, PWLRKEYBOARD->modifiers.depressed, PWLRKEYBOARD->modifiers.latched, PWLRKEYBOARD->modifiers.locked,
                                      activeLayout == 0 ? LAYOUTS - 1 : activeLayout - 1);
    } else {

        int requestedLayout = 0;
        try {
            requestedLayout = std::stoi(CMD);
        } catch (std::exception& e) { return "invalid arg 2"; }

        if (requestedLayout < 0 || (uint64_t)requestedLayout > LAYOUTS - 1) {
            return "layout idx out of range of " + std::to_string(LAYOUTS);
        }

        wlr_keyboard_notify_modifiers(PWLRKEYBOARD, PWLRKEYBOARD->modifiers.depressed, PWLRKEYBOARD->modifiers.latched, PWLRKEYBOARD->modifiers.locked, requestedLayout);
    }

    return "ok";
}

std::string dispatchSeterror(eHyprCtlOutputFormat format, std::string request) {
    CVarList    vars(request, 0, ' ');

    std::string errorMessage = "";

    if (vars.size() < 3) {
        g_pHyprError->destroy();

        if (vars.size() == 2 && !vars[1].find("dis"))
            return "var 1 not color or disable";

        return "ok";
    }

    const CColor COLOR = configStringToInt(vars[1]);

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

std::string dispatchSetProp(eHyprCtlOutputFormat format, std::string request) {
    CVarList vars(request, 0, ' ');

    if (vars.size() < 4)
        return "not enough args";

    const auto PLASTWINDOW = g_pCompositor->m_pLastWindow.lock();
    const auto PWINDOW     = g_pCompositor->getWindowByRegex(vars[1]);

    if (!PWINDOW)
        return "window not found";

    const auto PROP = vars[2];
    const auto VAL  = vars[3];

    auto       noFocus = PWINDOW->m_sAdditionalConfigData.noFocus;

    bool       lock = false;

    if (request.ends_with("lock"))
        lock = true;

    try {
        if (PROP == "animationstyle") {
            PWINDOW->m_sAdditionalConfigData.animationStyle = VAL;
        } else if (PROP == "rounding") {
            PWINDOW->m_sAdditionalConfigData.rounding.forceSetIgnoreLocked(configStringToInt(VAL), lock);
        } else if (PROP == "forcenoblur") {
            PWINDOW->m_sAdditionalConfigData.forceNoBlur.forceSetIgnoreLocked(configStringToInt(VAL), lock);
        } else if (PROP == "forceopaque") {
            PWINDOW->m_sAdditionalConfigData.forceOpaque.forceSetIgnoreLocked(configStringToInt(VAL), lock);
        } else if (PROP == "forceopaqueoverriden") {
            PWINDOW->m_sAdditionalConfigData.forceOpaqueOverridden.forceSetIgnoreLocked(configStringToInt(VAL), lock);
        } else if (PROP == "forceallowsinput") {
            PWINDOW->m_sAdditionalConfigData.forceAllowsInput.forceSetIgnoreLocked(configStringToInt(VAL), lock);
        } else if (PROP == "forcenoanims") {
            PWINDOW->m_sAdditionalConfigData.forceNoAnims.forceSetIgnoreLocked(configStringToInt(VAL), lock);
        } else if (PROP == "forcenoborder") {
            PWINDOW->m_sAdditionalConfigData.forceNoBorder.forceSetIgnoreLocked(configStringToInt(VAL), lock);
        } else if (PROP == "forcenoshadow") {
            PWINDOW->m_sAdditionalConfigData.forceNoShadow.forceSetIgnoreLocked(configStringToInt(VAL), lock);
        } else if (PROP == "forcenodim") {
            PWINDOW->m_sAdditionalConfigData.forceNoDim.forceSetIgnoreLocked(configStringToInt(VAL), lock);
        } else if (PROP == "nofocus") {
            PWINDOW->m_sAdditionalConfigData.noFocus.forceSetIgnoreLocked(configStringToInt(VAL), lock);
        } else if (PROP == "windowdancecompat") {
            PWINDOW->m_sAdditionalConfigData.windowDanceCompat.forceSetIgnoreLocked(configStringToInt(VAL), lock);
        } else if (PROP == "nomaxsize") {
            PWINDOW->m_sAdditionalConfigData.noMaxSize.forceSetIgnoreLocked(configStringToInt(VAL), lock);
        } else if (PROP == "maxsize") {
            PWINDOW->m_sAdditionalConfigData.maxSize.forceSetIgnoreLocked(configStringToVector2D(VAL + " " + vars[4]), lock);
            if (lock) {
                PWINDOW->m_vRealSize = Vector2D(std::min((double)PWINDOW->m_sAdditionalConfigData.maxSize.toUnderlying().x, PWINDOW->m_vRealSize.goal().x),
                                                std::min((double)PWINDOW->m_sAdditionalConfigData.maxSize.toUnderlying().y, PWINDOW->m_vRealSize.goal().y));
                g_pXWaylandManager->setWindowSize(PWINDOW, PWINDOW->m_vRealSize.goal());
                PWINDOW->setHidden(false);
            }
        } else if (PROP == "minsize") {
            PWINDOW->m_sAdditionalConfigData.minSize.forceSetIgnoreLocked(configStringToVector2D(VAL + " " + vars[4]), lock);
            if (lock) {
                PWINDOW->m_vRealSize = Vector2D(std::max((double)PWINDOW->m_sAdditionalConfigData.minSize.toUnderlying().x, PWINDOW->m_vRealSize.goal().x),
                                                std::max((double)PWINDOW->m_sAdditionalConfigData.minSize.toUnderlying().y, PWINDOW->m_vRealSize.goal().y));
                g_pXWaylandManager->setWindowSize(PWINDOW, PWINDOW->m_vRealSize.goal());
                PWINDOW->setHidden(false);
            }
        } else if (PROP == "dimaround") {
            PWINDOW->m_sAdditionalConfigData.dimAround.forceSetIgnoreLocked(configStringToInt(VAL), lock);
        } else if (PROP == "alphaoverride") {
            PWINDOW->m_sSpecialRenderData.alphaOverride.forceSetIgnoreLocked(configStringToInt(VAL), lock);
        } else if (PROP == "alpha") {
            PWINDOW->m_sSpecialRenderData.alpha.forceSetIgnoreLocked(std::stof(VAL), lock);
        } else if (PROP == "alphainactiveoverride") {
            PWINDOW->m_sSpecialRenderData.alphaInactiveOverride.forceSetIgnoreLocked(configStringToInt(VAL), lock);
        } else if (PROP == "alphainactive") {
            PWINDOW->m_sSpecialRenderData.alphaInactive.forceSetIgnoreLocked(std::stof(VAL), lock);
        } else if (PROP == "alphafullscreenoverride") {
            PWINDOW->m_sSpecialRenderData.alphaFullscreenOverride.forceSetIgnoreLocked(configStringToInt(VAL), lock);
        } else if (PROP == "alphafullscreen") {
            PWINDOW->m_sSpecialRenderData.alphaFullscreen.forceSetIgnoreLocked(std::stof(VAL), lock);
        } else if (PROP == "activebordercolor" || PROP == "inactivebordercolor") {
            CGradientValueData colorData = {};
            if (vars.size() > 4) {
                for (int i = 3; i < static_cast<int>(lock ? vars.size() - 1 : vars.size()); ++i) {
                    const auto TOKEN = vars[i];
                    if (TOKEN.ends_with("deg"))
                        colorData.m_fAngle = std::stoi(TOKEN.substr(0, TOKEN.size() - 3)) * (PI / 180.0);
                    else
                        colorData.m_vColors.push_back(configStringToInt(TOKEN));
                }
            } else if (VAL != "-1")
                colorData.m_vColors.push_back(configStringToInt(VAL));

            if (PROP == "activebordercolor")
                PWINDOW->m_sSpecialRenderData.activeBorderColor.forceSetIgnoreLocked(colorData, lock);
            else
                PWINDOW->m_sSpecialRenderData.inactiveBorderColor.forceSetIgnoreLocked(colorData, lock);
        } else if (PROP == "forcergbx") {
            PWINDOW->m_sAdditionalConfigData.forceRGBX.forceSetIgnoreLocked(configStringToInt(VAL), lock);
        } else if (PROP == "bordersize") {
            PWINDOW->m_sSpecialRenderData.borderSize.forceSetIgnoreLocked(configStringToInt(VAL), lock);
        } else if (PROP == "keepaspectratio") {
            PWINDOW->m_sAdditionalConfigData.keepAspectRatio.forceSetIgnoreLocked(configStringToInt(VAL), lock);
        } else if (PROP == "immediate") {
            PWINDOW->m_sAdditionalConfigData.forceTearing.forceSetIgnoreLocked(configStringToInt(VAL), lock);
        } else if (PROP == "nearestneighbor") {
            PWINDOW->m_sAdditionalConfigData.nearestNeighbor.forceSetIgnoreLocked(configStringToInt(VAL), lock);
        } else {
            return "prop not found";
        }
    } catch (std::exception& e) { return "error in parsing prop value: " + std::string(e.what()); }

    g_pCompositor->updateAllWindowsAnimatedDecorationValues();

    if (!(PWINDOW->m_sAdditionalConfigData.noFocus.toUnderlying() == noFocus.toUnderlying())) {
        g_pCompositor->focusWindow(nullptr);
        g_pCompositor->focusWindow(PWINDOW);
        g_pCompositor->focusWindow(PLASTWINDOW);
    }

    for (auto& m : g_pCompositor->m_vMonitors)
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(m->ID);

    return "ok";
}

std::string dispatchGetOption(eHyprCtlOutputFormat format, std::string request) {
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

        curitem = removeBeginEndSpacesTabs(curitem);
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
            return std::format("custom type: {}\nset: {}", ((ICustomConfigValueData*)std::any_cast<void*>(VAL))->toString(), VAR->m_bSetByUser);
    } else {
        if (TYPE == typeid(Hyprlang::INT))
            return std::format("{{\"option\": \"{}\", \"int\": {}, \"set\": {} }}", curitem, std::any_cast<Hyprlang::INT>(VAL), VAR->m_bSetByUser);
        else if (TYPE == typeid(Hyprlang::FLOAT))
            return std::format("{{\"option\": \"{}\", \"float\": {:2f}, \"set\": {} }}", curitem, std::any_cast<Hyprlang::FLOAT>(VAL), VAR->m_bSetByUser);
        else if (TYPE == typeid(Hyprlang::VEC2))
            return std::format("{{\"option\": \"{}\", \"vec2\": [{},{}], \"set\": {} }}", curitem, std::any_cast<Hyprlang::VEC2>(VAL).x, std::any_cast<Hyprlang::VEC2>(VAL).y,
                               VAR->m_bSetByUser);
        else if (TYPE == typeid(Hyprlang::STRING))
            return std::format("{{\"option\": \"{}\", \"str\": \"{}\", \"set\": {} }}", curitem, escapeJSONStrings(std::any_cast<Hyprlang::STRING>(VAL)), VAR->m_bSetByUser);
        else if (TYPE == typeid(void*))
            return std::format("{{\"option\": \"{}\", \"custom\": \"{}\", \"set\": {} }}", curitem, ((ICustomConfigValueData*)std::any_cast<void*>(VAL))->toString(),
                               VAR->m_bSetByUser);
    }

    return "invalid type (internal error)";
}

std::string decorationRequest(eHyprCtlOutputFormat format, std::string request) {
    CVarList   vars(request, 0, ' ');
    const auto PWINDOW = g_pCompositor->getWindowByRegex(vars[1]);

    if (!PWINDOW)
        return "none";

    std::string result = "";
    if (format == eHyprCtlOutputFormat::FORMAT_JSON) {
        result += "[";
        for (auto& wd : PWINDOW->m_dWindowDecorations) {
            result += "{\n\"decorationName\": \"" + wd->getDisplayName() + "\",\n\"priority\": " + std::to_string(wd->getPositioningInfo().priority) + "\n},";
        }

        trimTrailingComma(result);
        result += "]";
    } else {
        result = +"Decoration\tPriority\n";
        for (auto& wd : PWINDOW->m_dWindowDecorations) {
            result += wd->getDisplayName() + "\t" + std::to_string(wd->getPositioningInfo().priority) + "\n";
        }
    }

    return result;
}

void createOutputIter(wlr_backend* backend, void* data) {
    const auto DATA = (std::pair<std::string, bool>*)data;

    if (DATA->second)
        return;

    if (DATA->first.empty() || DATA->first == "auto") {
        if (wlr_backend_is_wl(backend)) {
            wlr_wl_output_create(backend);
            DATA->second = true;
        } else if (wlr_backend_is_x11(backend)) {
            wlr_x11_output_create(backend);
            DATA->second = true;
        } else if (wlr_backend_is_headless(backend)) {
            wlr_headless_add_output(backend, 1920, 1080);
            DATA->second = true;
        }
    } else {
        if (wlr_backend_is_wl(backend) && DATA->first == "wayland") {
            wlr_wl_output_create(backend);
            DATA->second = true;
        } else if (wlr_backend_is_x11(backend) && DATA->first == "x11") {
            wlr_x11_output_create(backend);
            DATA->second = true;
        } else if (wlr_backend_is_headless(backend) && DATA->first == "headless") {
            wlr_headless_add_output(backend, 1920, 1080);
            DATA->second = true;
        }
    }
}

std::string dispatchOutput(eHyprCtlOutputFormat format, std::string request) {
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

        curitem = removeBeginEndSpacesTabs(curitem);
    };

    nextItem();
    nextItem();

    const auto MODE = curitem;

    nextItem();

    const auto NAME = curitem;

    if (MODE == "create" || MODE == "add") {
        std::pair<std::string, bool> result = {NAME, false};

        wlr_multi_for_each_backend(g_pCompositor->m_sWLRBackend, createOutputIter, &result);

        if (!result.second)
            return "no backend replied to the request";

    } else if (MODE == "destroy" || MODE == "remove") {
        const auto PMONITOR = g_pCompositor->getMonitorFromName(NAME);

        if (!PMONITOR)
            return "output not found";

        if (!PMONITOR->createdByUser)
            return "cannot remove a real display. Use the monitor keyword.";

        wlr_output_destroy(PMONITOR->output);
    }

    return "ok";
}

std::string dispatchPlugin(eHyprCtlOutputFormat format, std::string request) {
    CVarList vars(request, 0, ' ');

    if (vars.size() < 2)
        return "not enough args";

    const auto OPERATION = vars[1];
    const auto PATH      = vars[2];

    if (OPERATION == "load") {
        if (vars.size() < 3)
            return "not enough args";

        const auto PLUGIN = g_pPluginSystem->loadPlugin(PATH);

        if (!PLUGIN)
            return "error in loading plugin, last error: " + g_pPluginSystem->m_szLastError;
    } else if (OPERATION == "unload") {
        if (vars.size() < 3)
            return "not enough args";

        const auto PLUGIN = g_pPluginSystem->getPluginByPath(PATH);

        if (!PLUGIN)
            return "plugin not loaded";

        g_pPluginSystem->unloadPlugin(PLUGIN);
    } else if (OPERATION == "list") {
        const auto PLUGINS = g_pPluginSystem->getAllPlugins();

        if (PLUGINS.size() == 0)
            return "no plugins loaded";

        std::string list = "";
        for (auto& p : PLUGINS) {
            list += std::format("\nPlugin {} by {}:\n\tHandle: {:x}\n\tVersion: {}\n\tDescription: {}\n", p->name, p->author, (uintptr_t)p->m_pHandle, p->version, p->description);
        }

        return list;
    } else {
        return "unknown opt";
    }

    return "ok";
}

std::string dispatchNotify(eHyprCtlOutputFormat format, std::string request) {
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

    if (icon > ICON_NONE || icon < 0) {
        icon = ICON_NONE;
    }

    const auto TIME = vars[2];
    int        time = 0;
    try {
        time = std::stoi(TIME);
    } catch (std::exception& e) { return "invalid arg 2"; }

    CColor color = configStringToInt(vars[3]);

    size_t msgidx   = 4;
    float  fontsize = 13.f;
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

    g_pHyprNotificationOverlay->addNotification(MESSAGE, color, time, (eIcons)icon, fontsize);

    return "ok";
}

std::string dispatchDismissNotify(eHyprCtlOutputFormat format, std::string request) {
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

std::string getIsLocked(eHyprCtlOutputFormat format, std::string request) {
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

    registerCommand(SHyprCtlCommand{"monitors", false, monitorsRequest});
    registerCommand(SHyprCtlCommand{"reload", false, reloadRequest});
    registerCommand(SHyprCtlCommand{"plugin", false, dispatchPlugin});
    registerCommand(SHyprCtlCommand{"notify", false, dispatchNotify});
    registerCommand(SHyprCtlCommand{"dismissnotify", false, dispatchDismissNotify});
    registerCommand(SHyprCtlCommand{"setprop", false, dispatchSetProp});
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

SP<SHyprCtlCommand> CHyprCtl::registerCommand(SHyprCtlCommand cmd) {
    return m_vCommands.emplace_back(makeShared<SHyprCtlCommand>(cmd));
}

void CHyprCtl::unregisterCommand(const SP<SHyprCtlCommand>& cmd) {
    std::erase(m_vCommands, cmd);
}

std::string CHyprCtl::getReply(std::string request) {
    auto format             = eHyprCtlOutputFormat::FORMAT_NORMAL;
    bool reloadAll          = false;
    m_sCurrentRequestParams = {};

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
                m_sCurrentRequestParams.all = true;
        }

        if (sepIndex < request.size())
            request = request.substr(sepIndex + 1); // remove flags and separator so we can compare the rest of the string
    }

    std::string result = "";

    // parse exact cmds first, then non-exact.
    for (auto& cmd : m_vCommands) {
        if (!cmd->exact)
            continue;

        if (cmd->name == request) {
            result = cmd->fn(format, request);
            break;
        }
    }

    if (result.empty())
        for (auto& cmd : m_vCommands) {
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
        g_pConfigManager->m_bWantsMonitorReload = true; // for monitor keywords

        g_pInputManager->setKeyboardLayout();     // update kb layout
        g_pInputManager->setPointerConfigs();     // update mouse cfgs
        g_pInputManager->setTouchDeviceConfigs(); // update touch device cfgs
        g_pInputManager->setTabletConfigs();      // update tablets

        static auto PLAYOUT = CConfigValue<std::string>("general:layout");

        g_pLayoutManager->switchToLayout(*PLAYOUT); // update layout

        g_pHyprOpenGL->m_bReloadScreenShader = true;

        for (auto& [m, rd] : g_pHyprOpenGL->m_mMonitorRenderResources) {
            rd.blurFBDirty = true;
        }

        for (auto& m : g_pCompositor->m_vMonitors) {
            g_pHyprRenderer->damageMonitor(m.get());
            g_pLayoutManager->getCurrentLayout()->recalculateMonitor(m->ID);
        }
    }

    return result;
}

std::string CHyprCtl::makeDynamicCall(const std::string& input) {
    return getReply(input);
}

int hyprCtlFDTick(int fd, uint32_t mask, void* data) {
    if (mask & WL_EVENT_ERROR || mask & WL_EVENT_HANGUP)
        return 0;

    sockaddr_in            clientAddress;
    socklen_t              clientSize = sizeof(clientAddress);

    const auto             ACCEPTEDCONNECTION = accept4(g_pHyprCtl->m_iSocketFD, (sockaddr*)&clientAddress, &clientSize, SOCK_CLOEXEC);

    std::array<char, 1024> readBuffer;

    fd_set                 fdset;
    FD_ZERO(&fdset);
    FD_SET(ACCEPTEDCONNECTION, &fdset);
    timeval timeout = {.tv_sec = 0, .tv_usec = 5000};
    auto    success = select(ACCEPTEDCONNECTION + 1, &fdset, nullptr, nullptr, &timeout);

    if (success <= 0) {
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

    write(ACCEPTEDCONNECTION, reply.c_str(), reply.length());

    close(ACCEPTEDCONNECTION);

    if (g_pConfigManager->m_bWantsMonitorReload)
        g_pConfigManager->ensureMonitorStatus();

    return 0;
}

void CHyprCtl::startHyprCtlSocket() {

    m_iSocketFD = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);

    if (m_iSocketFD < 0) {
        Debug::log(ERR, "Couldn't start the Hyprland Socket. (1) IPC will not work.");
        return;
    }

    sockaddr_un SERVERADDRESS = {.sun_family = AF_UNIX};

    std::string socketPath = g_pCompositor->m_szInstancePath + "/.socket.sock";

    strcpy(SERVERADDRESS.sun_path, socketPath.c_str());

    if (bind(m_iSocketFD, (sockaddr*)&SERVERADDRESS, SUN_LEN(&SERVERADDRESS)) < 0) {
        Debug::log(ERR, "Couldn't start the Hyprland Socket. (2) IPC will not work.");
        return;
    }

    // 10 max queued.
    listen(m_iSocketFD, 10);

    Debug::log(LOG, "Hypr socket started at {}", socketPath);

    wl_event_loop_add_fd(g_pCompositor->m_sWLEventLoop, m_iSocketFD, WL_EVENT_READABLE, hyprCtlFDTick, nullptr);
}
