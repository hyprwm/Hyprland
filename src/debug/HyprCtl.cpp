#include "HyprCtl.hpp"

#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>

#include <sstream>
#include <string>

static void trimTrailingComma(std::string& str) {
    if (!str.empty() && str.back() == ',')
        str.pop_back();
}

static std::string getWorkspaceNameFromSpecialID(const int workspaceID) {
    if (workspaceID == 0)
        return "";
    const auto* workspace = g_pCompositor->getWorkspaceByID(workspaceID);
    if (!workspace)
        return "";
    return workspace->m_szName;
}

std::string monitorsRequest(HyprCtl::eHyprCtlOutputFormat format) {
    std::string result = "";
    if (format == HyprCtl::FORMAT_JSON) {
        result += "[";

        for (auto& m : g_pCompositor->m_vMonitors) {
            if (!m->output)
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
    "activelyTearing": {}
}},)#",
                m->ID, escapeJSONStrings(m->szName), escapeJSONStrings(m->output->description ? m->output->description : ""), (m->output->make ? m->output->make : ""),
                (m->output->model ? m->output->model : ""), (m->output->serial ? m->output->serial : ""), (int)m->vecPixelSize.x, (int)m->vecPixelSize.y, m->refreshRate,
                (int)m->vecPosition.x, (int)m->vecPosition.y, m->activeWorkspace, escapeJSONStrings(g_pCompositor->getWorkspaceByID(m->activeWorkspace)->m_szName),
                m->specialWorkspaceID, escapeJSONStrings(getWorkspaceNameFromSpecialID(m->specialWorkspaceID)), (int)m->vecReservedTopLeft.x, (int)m->vecReservedTopLeft.y,
                (int)m->vecReservedBottomRight.x, (int)m->vecReservedBottomRight.y, m->scale, (int)m->transform, (m.get() == g_pCompositor->m_pLastMonitor ? "true" : "false"),
                (m->dpmsStatus ? "true" : "false"), (m->output->adaptive_sync_status == WLR_OUTPUT_ADAPTIVE_SYNC_ENABLED ? "true" : "false"),
                m->tearingState.activelyTearing ? "true" : "false");
        }

        trimTrailingComma(result);

        result += "]";
    } else {
        for (auto& m : g_pCompositor->m_vMonitors) {
            if (!m->output)
                continue;

            result +=
                std::format("Monitor {} (ID {}):\n\t{}x{}@{:.5f} at {}x{}\n\tdescription: {}\n\tmake: {}\n\tmodel: {}\n\tserial: {}\n\tactive workspace: {} ({})\n\tspecial "
                            "workspace: {} ({})\n\treserved: {} "
                            "{} {} {}\n\tscale: {:.2f}\n\ttransform: "
                            "{}\n\tfocused: {}\n\tdpmsStatus: {}\n\tvrr: {}\n\tactivelyTearing: {}\n\n",
                            m->szName, m->ID, (int)m->vecPixelSize.x, (int)m->vecPixelSize.y, m->refreshRate, (int)m->vecPosition.x, (int)m->vecPosition.y,
                            (m->output->description ? m->output->description : ""), (m->output->make ? m->output->make : ""), (m->output->model ? m->output->model : ""),
                            (m->output->serial ? m->output->serial : ""), m->activeWorkspace, g_pCompositor->getWorkspaceByID(m->activeWorkspace)->m_szName, m->specialWorkspaceID,
                            getWorkspaceNameFromSpecialID(m->specialWorkspaceID), (int)m->vecReservedTopLeft.x, (int)m->vecReservedTopLeft.y, (int)m->vecReservedBottomRight.x,
                            (int)m->vecReservedBottomRight.y, m->scale, (int)m->transform, (m.get() == g_pCompositor->m_pLastMonitor ? "yes" : "no"), (int)m->dpmsStatus,
                            (int)(m->output->adaptive_sync_status == WLR_OUTPUT_ADAPTIVE_SYNC_ENABLED), m->tearingState.activelyTearing);
        }
    }

    return result;
}

static std::string getGroupedData(CWindow* w, HyprCtl::eHyprCtlOutputFormat format) {
    const bool isJson = format == HyprCtl::FORMAT_JSON;
    if (!w->m_sGroupData.pNextWindow)
        return isJson ? "" : "0";

    std::vector<CWindow*> groupMembers;

    CWindow*              curr = w;
    do {
        groupMembers.push_back(curr);
        curr = curr->m_sGroupData.pNextWindow;
    } while (curr != w);

    const auto         comma = isJson ? ", " : ",";
    std::ostringstream result;

    bool               first = true;
    for (auto& gw : groupMembers) {
        if (first)
            first = false;
        else
            result << comma;
        if (isJson)
            result << std::format("\"0x{:x}\"", (uintptr_t)gw);
        else
            result << std::format("{:x}", (uintptr_t)gw);
    }

    return result.str();
}

static std::string getWindowData(CWindow* w, HyprCtl::eHyprCtlOutputFormat format) {
    if (format == HyprCtl::FORMAT_JSON) {
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
    "swallowing": "0x{:x}"
}},)#",
            (uintptr_t)w, (w->m_bIsMapped ? "true" : "false"), (w->isHidden() ? "true" : "false"), (int)w->m_vRealPosition.goalv().x, (int)w->m_vRealPosition.goalv().y,
            (int)w->m_vRealSize.goalv().x, (int)w->m_vRealSize.goalv().y, w->m_iWorkspaceID,
            escapeJSONStrings(w->m_iWorkspaceID == -1                                ? "" :
                                  g_pCompositor->getWorkspaceByID(w->m_iWorkspaceID) ? g_pCompositor->getWorkspaceByID(w->m_iWorkspaceID)->m_szName :
                                                                                       std::string("Invalid workspace " + std::to_string(w->m_iWorkspaceID))),
            ((int)w->m_bIsFloating == 1 ? "true" : "false"), (int64_t)w->m_iMonitorID, escapeJSONStrings(g_pXWaylandManager->getAppIDClass(w)),
            escapeJSONStrings(g_pXWaylandManager->getTitle(w)), escapeJSONStrings(w->m_szInitialClass), escapeJSONStrings(w->m_szInitialTitle), w->getPID(),
            ((int)w->m_bIsX11 == 1 ? "true" : "false"), (w->m_bPinned ? "true" : "false"), (w->m_bIsFullscreen ? "true" : "false"),
            (w->m_bIsFullscreen ? (g_pCompositor->getWorkspaceByID(w->m_iWorkspaceID) ? (int)g_pCompositor->getWorkspaceByID(w->m_iWorkspaceID)->m_efFullscreenMode : 0) : 0),
            w->m_bFakeFullscreenState ? "true" : "false", getGroupedData(w, format), (uintptr_t)w->m_pSwallowed);
    } else {
        return std::format(
            "Window {:x} -> {}:\n\tmapped: {}\n\thidden: {}\n\tat: {},{}\n\tsize: {},{}\n\tworkspace: {} ({})\n\tfloating: {}\n\tmonitor: {}\n\tclass: {}\n\ttitle: "
            "{}\n\tinitialClass: {}\n\tinitialTitle: {}\n\tpid: "
            "{}\n\txwayland: {}\n\tpinned: "
            "{}\n\tfullscreen: {}\n\tfullscreenmode: {}\n\tfakefullscreen: {}\n\tgrouped: {}\n\tswallowing: {:x}\n\n",
            (uintptr_t)w, w->m_szTitle, (int)w->m_bIsMapped, (int)w->isHidden(), (int)w->m_vRealPosition.goalv().x, (int)w->m_vRealPosition.goalv().y,
            (int)w->m_vRealSize.goalv().x, (int)w->m_vRealSize.goalv().y, w->m_iWorkspaceID,
            (w->m_iWorkspaceID == -1                                ? "" :
                 g_pCompositor->getWorkspaceByID(w->m_iWorkspaceID) ? g_pCompositor->getWorkspaceByID(w->m_iWorkspaceID)->m_szName :
                                                                      std::string("Invalid workspace " + std::to_string(w->m_iWorkspaceID))),
            (int)w->m_bIsFloating, (int64_t)w->m_iMonitorID, g_pXWaylandManager->getAppIDClass(w), g_pXWaylandManager->getTitle(w), w->m_szInitialClass, w->m_szInitialTitle,
            w->getPID(), (int)w->m_bIsX11, (int)w->m_bPinned, (int)w->m_bIsFullscreen,
            (w->m_bIsFullscreen ? (g_pCompositor->getWorkspaceByID(w->m_iWorkspaceID) ? g_pCompositor->getWorkspaceByID(w->m_iWorkspaceID)->m_efFullscreenMode : 0) : 0),
            (int)w->m_bFakeFullscreenState, getGroupedData(w, format), (uintptr_t)w->m_pSwallowed);
    }
}

std::string clientsRequest(HyprCtl::eHyprCtlOutputFormat format) {
    std::string result = "";
    if (format == HyprCtl::FORMAT_JSON) {
        result += "[";

        for (auto& w : g_pCompositor->m_vWindows) {
            result += getWindowData(w.get(), format);
        }

        trimTrailingComma(result);

        result += "]";
    } else {
        for (auto& w : g_pCompositor->m_vWindows) {
            result += getWindowData(w.get(), format);
        }
    }
    return result;
}

static std::string getWorkspaceData(CWorkspace* w, HyprCtl::eHyprCtlOutputFormat format) {
    const auto PLASTW   = w->getLastFocusedWindow();
    const auto PMONITOR = g_pCompositor->getMonitorFromID(w->m_iMonitorID);
    if (format == HyprCtl::FORMAT_JSON) {
        return std::format(R"#({{
    "id": {},
    "name": "{}",
    "monitor": "{}",
    "windows": {},
    "hasfullscreen": {},
    "lastwindow": "0x{:x}",
    "lastwindowtitle": "{}"
}})#",
                           w->m_iID, escapeJSONStrings(w->m_szName), escapeJSONStrings(PMONITOR ? PMONITOR->szName : "?"), g_pCompositor->getWindowsOnWorkspace(w->m_iID),
                           ((int)w->m_bHasFullscreenWindow == 1 ? "true" : "false"), (uintptr_t)PLASTW, PLASTW ? escapeJSONStrings(PLASTW->m_szTitle) : "");
    } else {
        return std::format("workspace ID {} ({}) on monitor {}:\n\twindows: {}\n\thasfullscreen: {}\n\tlastwindow: 0x{:x}\n\tlastwindowtitle: {}\n\n", w->m_iID, w->m_szName,
                           PMONITOR ? PMONITOR->szName : "?", g_pCompositor->getWindowsOnWorkspace(w->m_iID), (int)w->m_bHasFullscreenWindow, (uintptr_t)PLASTW,
                           PLASTW ? PLASTW->m_szTitle : "");
    }
}

static std::string getWorkspaceRuleData(const SWorkspaceRule& r, HyprCtl::eHyprCtlOutputFormat format) {
    const auto boolToString = [](const bool b) -> std::string { return b ? "true" : "false"; };
    if (format == HyprCtl::FORMAT_JSON) {
        const std::string monitor    = r.monitor.empty() ? "" : std::format(",\n    \"monitor\": \"{}\"", escapeJSONStrings(r.monitor));
        const std::string default_   = (bool)(r.isDefault) ? std::format(",\n    \"default\": {}", boolToString(r.isDefault)) : "";
        const std::string persistent = (bool)(r.isPersistent) ? std::format(",\n    \"persistent\": {}", boolToString(r.isPersistent)) : "";
        const std::string gapsIn     = (bool)(r.gapsIn) ? std::format(",\n    \"gapsIn\": {}", r.gapsIn.value()) : "";
        const std::string gapsOut    = (bool)(r.gapsOut) ? std::format(",\n    \"gapsOut\": {}", r.gapsOut.value()) : "";
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
        const std::string gapsIn     = std::format("\tgapsIn: {}\n", (bool)(r.gapsIn) ? std::to_string(r.gapsIn.value()) : "<unset>");
        const std::string gapsOut    = std::format("\tgapsOut: {}\n", (bool)(r.gapsOut) ? std::to_string(r.gapsOut.value()) : "<unset>");
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
std::string activeWorkspaceRequest(HyprCtl::eHyprCtlOutputFormat format) {
    if (!g_pCompositor->m_pLastMonitor)
        return "unsafe state";

    std::string result = "";
    auto        w      = g_pCompositor->getWorkspaceByID(g_pCompositor->m_pLastMonitor->activeWorkspace);

    if (!w)
        return "internal error";

    return getWorkspaceData(w, format);
}

std::string workspacesRequest(HyprCtl::eHyprCtlOutputFormat format) {
    std::string result = "";

    if (format == HyprCtl::FORMAT_JSON) {
        result += "[";
        for (auto& w : g_pCompositor->m_vWorkspaces) {
            result += getWorkspaceData(w.get(), format);
            result += ",";
        }

        trimTrailingComma(result);
        result += "]";
    } else {
        for (auto& w : g_pCompositor->m_vWorkspaces) {
            result += getWorkspaceData(w.get(), format);
        }
    }

    return result;
}

std::string workspaceRulesRequest(HyprCtl::eHyprCtlOutputFormat format) {
    std::string result = "";
    if (format == HyprCtl::FORMAT_JSON) {
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

std::string activeWindowRequest(HyprCtl::eHyprCtlOutputFormat format) {
    const auto PWINDOW = g_pCompositor->m_pLastWindow;

    if (!g_pCompositor->windowValidMapped(PWINDOW))
        return format == HyprCtl::FORMAT_JSON ? "{}" : "Invalid";

    auto result = getWindowData(PWINDOW, format);

    if (format == HyprCtl::FORMAT_JSON)
        result.pop_back();

    return result;
}

std::string layersRequest(HyprCtl::eHyprCtlOutputFormat format) {
    std::string result = "";

    if (format == HyprCtl::FORMAT_JSON) {
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

std::string devicesRequest(HyprCtl::eHyprCtlOutputFormat format) {
    std::string result = "";

    if (format == HyprCtl::FORMAT_JSON) {
        result += "{\n";
        result += "\"mice\": [\n";

        for (auto& m : g_pInputManager->m_lMice) {
            result += std::format(
                R"#(    {{
        "address": "0x{:x}",
        "name": "{}",
        "defaultSpeed": {:.5f}
    }},)#",
                (uintptr_t)&m, escapeJSONStrings(m.name),
                wlr_input_device_is_libinput(m.mouse) ? libinput_device_config_accel_get_default_speed((libinput_device*)wlr_libinput_get_device_handle(m.mouse)) : 0.f);
        }

        trimTrailingComma(result);
        result += "\n],\n";

        result += "\"keyboards\": [\n";
        for (auto& k : g_pInputManager->m_lKeyboards) {
            const auto KM = g_pInputManager->getActiveLayoutForKeyboard(&k);
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
                (uintptr_t)&k, escapeJSONStrings(k.name), escapeJSONStrings(k.currentRules.rules), escapeJSONStrings(k.currentRules.model),
                escapeJSONStrings(k.currentRules.layout), escapeJSONStrings(k.currentRules.variant), escapeJSONStrings(k.currentRules.options), escapeJSONStrings(KM),
                (k.active ? "true" : "false"));
        }

        trimTrailingComma(result);
        result += "\n],\n";

        result += "\"tablets\": [\n";

        for (auto& d : g_pInputManager->m_lTabletPads) {
            result += std::format(
                R"#(    {{
        "address": "0x{:x}",
        "type": "tabletPad",
        "belongsTo": {{
            "address": "0x{:x}",
            "name": "{}"
        }}
    }},)#",
                (uintptr_t)&d, (uintptr_t)d.pTabletParent, escapeJSONStrings(d.pTabletParent ? d.pTabletParent->name : ""));
        }

        for (auto& d : g_pInputManager->m_lTablets) {
            result += std::format(
                R"#(    {{
        "address": "0x{:x}",
        "name": "{}"
    }},)#",
                (uintptr_t)&d, escapeJSONStrings(d.name));
        }

        for (auto& d : g_pInputManager->m_lTabletTools) {
            result += std::format(
                R"#(    {{
        "address": "0x{:x}",
        "type": "tabletTool",
        "belongsTo": "0x{:x}"
    }},)#",
                (uintptr_t)&d, d.wlrTabletTool ? (uintptr_t)d.wlrTabletTool->data : 0);
        }

        trimTrailingComma(result);
        result += "\n],\n";

        result += "\"touch\": [\n";

        for (auto& d : g_pInputManager->m_lTouchDevices) {
            result += std::format(
                R"#(    {{
        "address": "0x{:x}",
        "name": "{}"
    }},)#",
                (uintptr_t)&d, d.name);
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
                (uintptr_t)&d, d.pWlrDevice ? d.pWlrDevice->name : "");
        }

        trimTrailingComma(result);
        result += "\n]\n";

        result += "}\n";

    } else {
        result += "mice:\n";

        for (auto& m : g_pInputManager->m_lMice) {
            result += std::format(
                "\tMouse at {:x}:\n\t\t{}\n\t\t\tdefault speed: {:.5f}\n", (uintptr_t)&m, m.name,
                (wlr_input_device_is_libinput(m.mouse) ? libinput_device_config_accel_get_default_speed((libinput_device*)wlr_libinput_get_device_handle(m.mouse)) : 0.f));
        }

        result += "\n\nKeyboards:\n";

        for (auto& k : g_pInputManager->m_lKeyboards) {
            const auto KM = g_pInputManager->getActiveLayoutForKeyboard(&k);
            result += std::format("\tKeyboard at {:x}:\n\t\t{}\n\t\t\trules: r \"{}\", m \"{}\", l \"{}\", v \"{}\", o \"{}\"\n\t\t\tactive keymap: {}\n\t\t\tmain: {}\n",
                                  (uintptr_t)&k, k.name, k.currentRules.rules, k.currentRules.model, k.currentRules.layout, k.currentRules.variant, k.currentRules.options, KM,
                                  (k.active ? "yes" : "no"));
        }

        result += "\n\nTablets:\n";

        for (auto& d : g_pInputManager->m_lTabletPads) {
            result += std::format("\tTablet Pad at {:x} (belongs to {:x} -> {})\n", (uintptr_t)&d, (uintptr_t)d.pTabletParent, d.pTabletParent ? d.pTabletParent->name : "");
        }

        for (auto& d : g_pInputManager->m_lTablets) {
            result += std::format("\tTablet at {:x}:\n\t\t{}\n", (uintptr_t)&d, d.name);
        }

        for (auto& d : g_pInputManager->m_lTabletTools) {
            result += std::format("\tTablet Tool at {:x} (belongs to {:x})\n", (uintptr_t)&d, d.wlrTabletTool ? (uintptr_t)d.wlrTabletTool->data : 0);
        }

        result += "\n\nTouch:\n";

        for (auto& d : g_pInputManager->m_lTouchDevices) {
            result += std::format("\tTouch Device at {:x}:\n\t\t{}\n", (uintptr_t)&d, d.name);
        }

        result += "\n\nSwitches:\n";

        for (auto& d : g_pInputManager->m_lSwitches) {
            result += std::format("\tSwitch Device at {:x}:\n\t\t{}\n", (uintptr_t)&d, d.pWlrDevice ? d.pWlrDevice->name : "");
        }
    }

    return result;
}

std::string animationsRequest(HyprCtl::eHyprCtlOutputFormat format) {
    std::string ret = "";
    if (format == HyprCtl::eHyprCtlOutputFormat::FORMAT_NORMAL) {
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
                               ac.first, ac.second.overridden ? "true" : "false", ac.second.internalBezier, ac.second.internalEnabled ? "true" : "false", ac.second.internalSpeed,
                               ac.second.internalStyle);
        }

        ret[ret.length() - 1] = ']';

        ret += ",\n[";

        for (auto& bz : g_pAnimationManager->getAllBeziers()) {
            ret += std::format(R"#(
{{
    "name": "{}"
}},)#",
                               bz.first);
        }

        trimTrailingComma(ret);

        ret += "]]";
    }

    return ret;
}

std::string globalShortcutsRequest(HyprCtl::eHyprCtlOutputFormat format) {
    std::string ret       = "";
    const auto  SHORTCUTS = g_pProtocolManager->m_pGlobalShortcutsProtocolManager->getAllShortcuts();
    if (format == HyprCtl::eHyprCtlOutputFormat::FORMAT_NORMAL) {
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

std::string bindsRequest(HyprCtl::eHyprCtlOutputFormat format) {
    std::string ret = "";
    if (format == HyprCtl::eHyprCtlOutputFormat::FORMAT_NORMAL) {
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

            ret += std::format("\n\tmodmask: {}\n\tsubmap: {}\n\tkey: {}\n\tkeycode: {}\n\tdispatcher: {}\n\targ: {}\n\n", kb.modmask, kb.submap, kb.key, kb.keycode, kb.handler,
                               kb.arg);
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
    "dispatcher": "{}",
    "arg": "{}"
}},)#",
                kb.locked ? "true" : "false", kb.mouse ? "true" : "false", kb.release ? "true" : "false", kb.repeat ? "true" : "false", kb.nonConsuming ? "true" : "false",
                kb.modmask, escapeJSONStrings(kb.submap), escapeJSONStrings(kb.key), kb.keycode, escapeJSONStrings(kb.handler), escapeJSONStrings(kb.arg));
        }
        trimTrailingComma(ret);
        ret += "]";
    }

    return ret;
}

std::string versionRequest(HyprCtl::eHyprCtlOutputFormat format) {

    auto commitMsg = removeBeginEndSpacesTabs(GIT_COMMIT_MESSAGE);
    std::replace(commitMsg.begin(), commitMsg.end(), '#', ' ');

    if (format == HyprCtl::eHyprCtlOutputFormat::FORMAT_NORMAL) {
        std::string result = "Hyprland, built from branch " + std::string(GIT_BRANCH) + " at commit " + GIT_COMMIT_HASH + " " + GIT_DIRTY + " (" + commitMsg +
            ").\nTag: " + GIT_TAG + "\n\nflags: (if any)\n";

#ifdef LEGACY_RENDERER
        result += "legacyrenderer\n";
#endif
#ifndef NDEBUG
        result += "debug\n";
#endif
#ifdef HYPRLAND_DEBUG
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
    "tag": "{}",
    "flags": [)#",
            GIT_BRANCH, GIT_COMMIT_HASH, (strcmp(GIT_DIRTY, "dirty") == 0 ? "true" : "false"), escapeJSONStrings(commitMsg), GIT_TAG);

#ifdef LEGACY_RENDERER
        result += "\"legacyrenderer\",";
#endif
#ifndef NDEBUG
        result += "\"debug\",";
#endif
#ifdef HYPRLAND_DEBUG
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

std::string dispatchRequest(std::string in) {
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

std::string dispatchKeyword(std::string in) {
    // get rid of the keyword keyword
    in = in.substr(in.find_first_of(' ') + 1);

    const auto  COMMAND = in.substr(0, in.find_first_of(' '));

    const auto  VALUE = in.substr(in.find_first_of(' ') + 1);

    std::string retval = g_pConfigManager->parseKeyword(COMMAND, VALUE, true);

    if (COMMAND == "monitor")
        g_pConfigManager->m_bWantsMonitorReload = true; // for monitor keywords

    if (COMMAND.contains("input") || COMMAND.contains("device:")) {
        g_pInputManager->setKeyboardLayout();     // update kb layout
        g_pInputManager->setPointerConfigs();     // update mouse cfgs
        g_pInputManager->setTouchDeviceConfigs(); // update touch device cfgs
        g_pInputManager->setTabletConfigs();      // update tablets
    }

    if (COMMAND.contains("general:layout"))
        g_pLayoutManager->switchToLayout(g_pConfigManager->getString("general:layout")); // update layout

    if (COMMAND.contains("decoration:screen_shader"))
        g_pHyprOpenGL->m_bReloadScreenShader = true;

    if (COMMAND.contains("blur")) {
        for (auto& [m, rd] : g_pHyprOpenGL->m_mMonitorRenderResources) {
            rd.blurFBDirty = true;
        }
    }

    // decorations will probably need a repaint
    if (COMMAND.contains("decoration:") || COMMAND.contains("border") || COMMAND == "workspace" || COMMAND.contains("cursor_zoom_factor")) {
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

std::string reloadRequest(const std::string& request) {

    const auto REQMODE = request.substr(request.find_last_of(' ') + 1);

    g_pConfigManager->m_bForceReload = true;

    if (REQMODE == "config-only") {
        g_pConfigManager->m_bNoMonitorReload = true;
    }

    g_pConfigManager->tick();

    return "ok";
}

std::string killRequest() {
    g_pInputManager->setClickMode(CLICKMODE_KILL);

    return "ok";
}

std::string splashRequest() {
    return g_pCompositor->m_szCurrentSplash;
}

std::string cursorPosRequest(HyprCtl::eHyprCtlOutputFormat format) {
    const auto CURSORPOS = g_pInputManager->getMouseCoordsInternal().floor();

    if (format == HyprCtl::FORMAT_NORMAL) {
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

std::string getReply(std::string);

std::string dispatchBatch(std::string request) {
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

    while (curitem != "") {
        reply += getReply(curitem);

        nextItem();
    }

    return reply;
}

std::string dispatchSetCursor(std::string request) {
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

    wlr_xcursor_manager_destroy(g_pCompositor->m_sWLRXCursorMgr);

    g_pCompositor->m_sWLRXCursorMgr = wlr_xcursor_manager_create(theme.c_str(), size);

    setenv("XCURSOR_SIZE", SIZESTR.c_str(), true);
    setenv("XCURSOR_THEME", theme.c_str(), true);

    for (auto& m : g_pCompositor->m_vMonitors) {
        wlr_xcursor_manager_load(g_pCompositor->m_sWLRXCursorMgr, m->scale);
    }

    return "ok";
}

std::string switchXKBLayoutRequest(const std::string& request) {
    CVarList   vars(request, 0, ' ');

    const auto KB  = vars[1];
    const auto CMD = vars[2];

    // get kb
    const auto PKEYBOARD = std::find_if(g_pInputManager->m_lKeyboards.begin(), g_pInputManager->m_lKeyboards.end(),
                                        [&](const SKeyboard& other) { return other.name == g_pInputManager->deviceNameToInternalString(KB); });

    if (PKEYBOARD == g_pInputManager->m_lKeyboards.end())
        return "device not found";

    const auto         PWLRKEYBOARD = wlr_keyboard_from_input_device(PKEYBOARD->keyboard);
    const auto         LAYOUTS      = xkb_keymap_num_layouts(PWLRKEYBOARD->keymap);
    xkb_layout_index_t activeLayout = 0;
    while (activeLayout < LAYOUTS) {
        if (xkb_state_layout_index_is_active(PWLRKEYBOARD->xkb_state, activeLayout, XKB_STATE_LAYOUT_EFFECTIVE))
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

std::string dispatchSeterror(std::string request) {
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

std::string dispatchSetProp(std::string request) {
    CVarList vars(request, 0, ' ');

    if (vars.size() < 4)
        return "not enough args";

    const auto PWINDOW = g_pCompositor->getWindowByRegex(vars[1]);

    if (!PWINDOW)
        return "window not found";

    const auto PROP = vars[2];
    const auto VAL  = vars[3];

    bool       lock = false;

    if (vars.size() > 4) {
        if (vars[4].starts_with("lock")) {
            lock = true;
        }
    }

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
        } else if (PROP == "windowdancecompat") {
            PWINDOW->m_sAdditionalConfigData.windowDanceCompat.forceSetIgnoreLocked(configStringToInt(VAL), lock);
        } else if (PROP == "nomaxsize") {
            PWINDOW->m_sAdditionalConfigData.noMaxSize.forceSetIgnoreLocked(configStringToInt(VAL), lock);
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
        } else if (PROP == "activebordercolor") {
            PWINDOW->m_sSpecialRenderData.activeBorderColor.forceSetIgnoreLocked(configStringToInt(VAL), lock);
        } else if (PROP == "inactivebordercolor") {
            PWINDOW->m_sSpecialRenderData.inactiveBorderColor.forceSetIgnoreLocked(configStringToInt(VAL), lock);
        } else if (PROP == "forcergbx") {
            PWINDOW->m_sAdditionalConfigData.forceRGBX.forceSetIgnoreLocked(configStringToInt(VAL), lock);
        } else if (PROP == "bordersize") {
            PWINDOW->m_sSpecialRenderData.borderSize.forceSetIgnoreLocked(configStringToInt(VAL), lock);
        } else if (PROP == "keepaspectratio") {
            PWINDOW->m_sAdditionalConfigData.keepAspectRatio.forceSetIgnoreLocked(configStringToInt(VAL), lock);
        } else if (PROP == "immediate") {
            PWINDOW->m_sAdditionalConfigData.forceTearing.forceSetIgnoreLocked(configStringToInt(VAL), lock);
        } else {
            return "prop not found";
        }
    } catch (std::exception& e) { return "error in parsing prop value: " + std::string(e.what()); }

    g_pCompositor->updateAllWindowsAnimatedDecorationValues();

    for (auto& m : g_pCompositor->m_vMonitors)
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(m->ID);

    return "ok";
}

std::string dispatchGetOption(std::string request, HyprCtl::eHyprCtlOutputFormat format) {
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

    const auto PCFGOPT = g_pConfigManager->getConfigValuePtrSafe(curitem);

    if (!PCFGOPT)
        return "no such option";

    if (format == HyprCtl::eHyprCtlOutputFormat::FORMAT_NORMAL)
        return std::format("option {}\n\tint: {}\n\tfloat: {:.5f}\n\tstr: \"{}\"\n\tdata: {:x}\n\tset: {}", curitem, PCFGOPT->intValue, PCFGOPT->floatValue, PCFGOPT->strValue,
                           (uintptr_t)PCFGOPT->data.get(), PCFGOPT->set);
    else {
        return std::format(
            R"#(
{{
    "option": "{}",
    "int": {},
    "float": {:.5f},
    "str": "{}",
    "data": "0x{:x}",
    "set": {}
}}
)#",
            curitem, PCFGOPT->intValue, PCFGOPT->floatValue, PCFGOPT->strValue, (uintptr_t)PCFGOPT->data.get(), PCFGOPT->set);
    }
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

std::string dispatchOutput(std::string request) {
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

std::string dispatchPlugin(std::string request) {
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
            return "error in loading plugin";
    } else if (OPERATION == "unload") {
        if (vars.size() < 3)
            return "not enough args";

        const auto PLUGIN = g_pPluginSystem->getPluginByPath(PATH);

        if (!PLUGIN)
            return "plugin not loaded";

        g_pPluginSystem->unloadPlugin(PLUGIN);
    } else if (OPERATION == "list") {
        const auto  PLUGINS = g_pPluginSystem->getAllPlugins();

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

std::string dispatchNotify(std::string request) {
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

    CColor      color = configStringToInt(vars[3]);

    std::string message = "";

    for (size_t i = 4; i < vars.size(); ++i) {
        message += vars[i] + " ";
    }

    message.pop_back();

    g_pHyprNotificationOverlay->addNotification(message, color, time, (eIcons)icon);

    return "ok";
}

std::string getReply(std::string request) {
    auto format = HyprCtl::FORMAT_NORMAL;

    // process flags for non-batch requests
    if (!request.contains("[[BATCH]]") && request.contains("/")) {
        long unsigned int sepIndex = 0;
        for (const auto& c : request) {
            if (c == '/') { // stop at separator
                break;
            }

            sepIndex++;

            if (c == 'j')
                format = HyprCtl::FORMAT_JSON;
        }

        if (sepIndex < request.size())
            request = request.substr(sepIndex + 1); // remove flags and separator so we can compare the rest of the string
    }

    if (request == "monitors")
        return monitorsRequest(format);
    else if (request == "workspaces")
        return workspacesRequest(format);
    else if (request == "workspacerules")
        return workspaceRulesRequest(format);
    else if (request == "activeworkspace")
        return activeWorkspaceRequest(format);
    else if (request == "clients")
        return clientsRequest(format);
    else if (request == "kill")
        return killRequest();
    else if (request == "activewindow")
        return activeWindowRequest(format);
    else if (request == "layers")
        return layersRequest(format);
    else if (request == "version")
        return versionRequest(format);
    else if (request.starts_with("reload"))
        return reloadRequest(request);
    else if (request == "devices")
        return devicesRequest(format);
    else if (request == "splash")
        return splashRequest();
    else if (request == "cursorpos")
        return cursorPosRequest(format);
    else if (request == "binds")
        return bindsRequest(format);
    else if (request == "globalshortcuts")
        return globalShortcutsRequest(format);
    else if (request == "animations")
        return animationsRequest(format);
    else if (request.starts_with("plugin"))
        return dispatchPlugin(request);
    else if (request.starts_with("notify"))
        return dispatchNotify(request);
    else if (request.starts_with("setprop"))
        return dispatchSetProp(request);
    else if (request.starts_with("seterror"))
        return dispatchSeterror(request);
    else if (request.starts_with("switchxkblayout"))
        return switchXKBLayoutRequest(request);
    else if (request.starts_with("output"))
        return dispatchOutput(request);
    else if (request.starts_with("dispatch"))
        return dispatchRequest(request);
    else if (request.starts_with("keyword"))
        return dispatchKeyword(request);
    else if (request.starts_with("setcursor"))
        return dispatchSetCursor(request);
    else if (request.starts_with("getoption"))
        return dispatchGetOption(request, format);
    else if (request.starts_with("[[BATCH]]"))
        return dispatchBatch(request);

    return "unknown request";
}

std::string HyprCtl::makeDynamicCall(const std::string& input) {
    return getReply(input);
}

int hyprCtlFDTick(int fd, uint32_t mask, void* data) {
    if (mask & WL_EVENT_ERROR || mask & WL_EVENT_HANGUP)
        return 0;

    sockaddr_in clientAddress;
    socklen_t   clientSize = sizeof(clientAddress);

    const auto  ACCEPTEDCONNECTION = accept4(HyprCtl::iSocketFD, (sockaddr*)&clientAddress, &clientSize, SOCK_CLOEXEC);

    char        readBuffer[1024];

    fd_set      fdset;
    FD_ZERO(&fdset);
    FD_SET(ACCEPTEDCONNECTION, &fdset);
    timeval timeout = {.tv_sec = 0, .tv_usec = 5000};
    auto    success = select(ACCEPTEDCONNECTION + 1, &fdset, nullptr, nullptr, &timeout);

    if (success <= 0) {
        close(ACCEPTEDCONNECTION);
        return 0;
    }

    auto messageSize                                     = read(ACCEPTEDCONNECTION, readBuffer, 1024);
    readBuffer[messageSize == 1024 ? 1023 : messageSize] = '\0';

    std::string request(readBuffer);

    std::string reply = "";

    try {
        reply = getReply(request);
    } catch (std::exception& e) {
        Debug::log(ERR, "Error in request: {}", e.what());
        reply = "Err: " + std::string(e.what());
    }

    write(ACCEPTEDCONNECTION, reply.c_str(), reply.length());

    close(ACCEPTEDCONNECTION);

    if (g_pConfigManager->m_bWantsMonitorReload) {
        g_pConfigManager->ensureMonitorStatus();
    }

    return 0;
}

void HyprCtl::startHyprCtlSocket() {

    iSocketFD = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);

    if (iSocketFD < 0) {
        Debug::log(ERR, "Couldn't start the Hyprland Socket. (1) IPC will not work.");
        return;
    }

    sockaddr_un SERVERADDRESS = {.sun_family = AF_UNIX};

    std::string socketPath = "/tmp/hypr/" + g_pCompositor->m_szInstanceSignature + "/.socket.sock";

    strcpy(SERVERADDRESS.sun_path, socketPath.c_str());

    if (bind(iSocketFD, (sockaddr*)&SERVERADDRESS, SUN_LEN(&SERVERADDRESS)) < 0) {
        Debug::log(ERR, "Couldn't start the Hyprland Socket. (2) IPC will not work.");
        return;
    }

    // 10 max queued.
    listen(iSocketFD, 10);

    Debug::log(LOG, "Hypr socket started at {}", socketPath);

    wl_event_loop_add_fd(g_pCompositor->m_sWLEventLoop, iSocketFD, WL_EVENT_READABLE, hyprCtlFDTick, nullptr);
}
