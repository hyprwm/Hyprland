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

std::string monitorsRequest(HyprCtl::eHyprCtlOutputFormat format) {
    std::string result = "";
    if (format == HyprCtl::FORMAT_JSON) {
        result += "[";

        for (auto& m : g_pCompositor->m_vMonitors) {
            if (!m->output)
                continue;

            result += getFormat(
                R"#({
    "id": %i,
    "name": "%s",
    "description": "%s",
    "make": "%s",
    "model": "%s",
    "serial": "%s",
    "width": %i,
    "height": %i,
    "refreshRate": %f,
    "x": %i,
    "y": %i,
    "activeWorkspace": {
        "id": %i,
        "name": "%s"
    },
    "reserved": [%i, %i, %i, %i],
    "scale": %.2f,
    "transform": %i,
    "focused": %s,
    "dpmsStatus": %s,
    "vrr": %s
},)#",
                m->ID, escapeJSONStrings(m->szName).c_str(), escapeJSONStrings(m->output->description ? m->output->description : "").c_str(),
                (m->output->make ? m->output->make : ""), (m->output->model ? m->output->model : ""), (m->output->serial ? m->output->serial : ""), (int)m->vecPixelSize.x,
                (int)m->vecPixelSize.y, m->refreshRate, (int)m->vecPosition.x, (int)m->vecPosition.y, m->activeWorkspace,
                escapeJSONStrings(g_pCompositor->getWorkspaceByID(m->activeWorkspace)->m_szName).c_str(), (int)m->vecReservedTopLeft.x, (int)m->vecReservedTopLeft.y,
                (int)m->vecReservedBottomRight.x, (int)m->vecReservedBottomRight.y, m->scale, (int)m->transform, (m.get() == g_pCompositor->m_pLastMonitor ? "true" : "false"),
                (m->dpmsStatus ? "true" : "false"), (m->output->adaptive_sync_status == WLR_OUTPUT_ADAPTIVE_SYNC_ENABLED ? "true" : "false"));
        }

        // remove trailing comma
        result.pop_back();

        result += "]";
    } else {
        for (auto& m : g_pCompositor->m_vMonitors) {
            if (!m->output)
                continue;

            result += getFormat("Monitor %s (ID %i):\n\t%ix%i@%f at %ix%i\n\tdescription: %s\n\tmake: %s\n\tmodel: %s\n\tserial: %s\n\tactive workspace: %i (%s)\n\treserved: %i "
                                "%i %i %i\n\tscale: %.2f\n\ttransform: "
                                "%i\n\tfocused: %s\n\tdpmsStatus: %i\n\tvrr: %i\n\n",
                                m->szName.c_str(), m->ID, (int)m->vecPixelSize.x, (int)m->vecPixelSize.y, m->refreshRate, (int)m->vecPosition.x, (int)m->vecPosition.y,
                                (m->output->description ? m->output->description : ""), (m->output->make ? m->output->make : ""), (m->output->model ? m->output->model : ""),
                                (m->output->serial ? m->output->serial : ""), m->activeWorkspace, g_pCompositor->getWorkspaceByID(m->activeWorkspace)->m_szName.c_str(),
                                (int)m->vecReservedTopLeft.x, (int)m->vecReservedTopLeft.y, (int)m->vecReservedBottomRight.x, (int)m->vecReservedBottomRight.y, m->scale,
                                (int)m->transform, (m.get() == g_pCompositor->m_pLastMonitor ? "yes" : "no"), (int)m->dpmsStatus,
                                (int)(m->output->adaptive_sync_status == WLR_OUTPUT_ADAPTIVE_SYNC_ENABLED));
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
    const auto         fmt   = isJson ? "\"0x%x\"" : "%x";
    std::ostringstream result;

    bool               first = true;
    for (auto& gw : groupMembers) {
        if (first)
            first = false;
        else
            result << comma;

        result << getFormat(fmt, gw);
    }

    return result.str();
}

static std::string getWindowData(CWindow* w, HyprCtl::eHyprCtlOutputFormat format) {
    if (format == HyprCtl::FORMAT_JSON) {
        return getFormat(
            R"#({
    "address": "0x%x",
    "mapped": %s,
    "hidden": %s,
    "at": [%i, %i],
    "size": [%i, %i],
    "workspace": {
        "id": %i,
        "name": "%s"
    },
    "floating": %s,
    "monitor": %i,
    "class": "%s",
    "title": "%s",
    "initialClass": "%s",
    "initialTitle": "%s",
    "pid": %i,
    "xwayland": %s,
    "pinned": %s,
    "fullscreen": %s,
    "fullscreenMode": %i,
    "fakeFullscreen": %s,
    "grouped": [%s],
    "swallowing": %s
},)#",
            w, (w->m_bIsMapped ? "true" : "false"), (w->isHidden() ? "true" : "false"), (int)w->m_vRealPosition.goalv().x, (int)w->m_vRealPosition.goalv().y,
            (int)w->m_vRealSize.goalv().x, (int)w->m_vRealSize.goalv().y, w->m_iWorkspaceID,
            escapeJSONStrings(w->m_iWorkspaceID == -1                                ? "" :
                                  g_pCompositor->getWorkspaceByID(w->m_iWorkspaceID) ? g_pCompositor->getWorkspaceByID(w->m_iWorkspaceID)->m_szName :
                                                                                       std::string("Invalid workspace " + std::to_string(w->m_iWorkspaceID)))
                .c_str(),
            ((int)w->m_bIsFloating == 1 ? "true" : "false"), w->m_iMonitorID, escapeJSONStrings(g_pXWaylandManager->getAppIDClass(w)).c_str(),
            escapeJSONStrings(g_pXWaylandManager->getTitle(w)).c_str(), escapeJSONStrings(w->m_szInitialClass).c_str(), escapeJSONStrings(w->m_szInitialTitle).c_str(), w->getPID(),
            ((int)w->m_bIsX11 == 1 ? "true" : "false"), (w->m_bPinned ? "true" : "false"), (w->m_bIsFullscreen ? "true" : "false"),
            (w->m_bIsFullscreen ? (g_pCompositor->getWorkspaceByID(w->m_iWorkspaceID) ? g_pCompositor->getWorkspaceByID(w->m_iWorkspaceID)->m_efFullscreenMode : 0) : 0),
            w->m_bFakeFullscreenState ? "true" : "false", getGroupedData(w, format).c_str(), (w->m_pSwallowed ? getFormat("\"0x%x\"", w->m_pSwallowed).c_str() : "null"));
    } else {
        return getFormat(
            "Window %x -> %s:\n\tmapped: %i\n\thidden: %i\n\tat: %i,%i\n\tsize: %i,%i\n\tworkspace: %i (%s)\n\tfloating: %i\n\tmonitor: %i\n\tclass: %s\n\ttitle: "
            "%s\n\tinitialClass: %s\n\tinitialTitle: %s\n\tpid: "
            "%i\n\txwayland: %i\n\tpinned: "
            "%i\n\tfullscreen: %i\n\tfullscreenmode: %i\n\tfakefullscreen: %i\n\tgrouped: %s\n\tswallowing: %x\n\n",
            w, w->m_szTitle.c_str(), (int)w->m_bIsMapped, (int)w->isHidden(), (int)w->m_vRealPosition.goalv().x, (int)w->m_vRealPosition.goalv().y, (int)w->m_vRealSize.goalv().x,
            (int)w->m_vRealSize.goalv().y, w->m_iWorkspaceID,
            (w->m_iWorkspaceID == -1                                ? "" :
                 g_pCompositor->getWorkspaceByID(w->m_iWorkspaceID) ? g_pCompositor->getWorkspaceByID(w->m_iWorkspaceID)->m_szName.c_str() :
                                                                      std::string("Invalid workspace " + std::to_string(w->m_iWorkspaceID)).c_str()),
            (int)w->m_bIsFloating, w->m_iMonitorID, g_pXWaylandManager->getAppIDClass(w).c_str(), g_pXWaylandManager->getTitle(w).c_str(), w->m_szInitialClass.c_str(),
            w->m_szInitialTitle.c_str(), w->getPID(), (int)w->m_bIsX11, (int)w->m_bPinned, (int)w->m_bIsFullscreen,
            (w->m_bIsFullscreen ? (g_pCompositor->getWorkspaceByID(w->m_iWorkspaceID) ? g_pCompositor->getWorkspaceByID(w->m_iWorkspaceID)->m_efFullscreenMode : 0) : 0),
            (int)w->m_bFakeFullscreenState, getGroupedData(w, format).c_str(), w->m_pSwallowed);
    }
}

std::string clientsRequest(HyprCtl::eHyprCtlOutputFormat format) {
    std::string result = "";
    if (format == HyprCtl::FORMAT_JSON) {
        result += "[";

        for (auto& w : g_pCompositor->m_vWindows) {
            result += getWindowData(w.get(), format);
        }

        // remove trailing comma
        if (result != "[")
            result.pop_back();

        result += "]";
    } else {
        for (auto& w : g_pCompositor->m_vWindows) {
            result += getWindowData(w.get(), format);
        }
    }
    return result;
}

std::string workspacesRequest(HyprCtl::eHyprCtlOutputFormat format) {
    std::string result = "";
    if (format == HyprCtl::FORMAT_JSON) {
        result += "[";

        for (auto& w : g_pCompositor->m_vWorkspaces) {
            const auto PLASTW = w->getLastFocusedWindow();

            result += getFormat(
                R"#({
    "id": %i,
    "name": "%s",
    "monitor": "%s",
    "windows": %i,
    "hasfullscreen": %s,
    "lastwindow": "0x%x",
    "lastwindowtitle": "%s"
},)#",
                w->m_iID, escapeJSONStrings(w->m_szName).c_str(), escapeJSONStrings(g_pCompositor->getMonitorFromID(w->m_iMonitorID)->szName).c_str(),
                g_pCompositor->getWindowsOnWorkspace(w->m_iID), ((int)w->m_bHasFullscreenWindow == 1 ? "true" : "false"), PLASTW,
                PLASTW ? escapeJSONStrings(PLASTW->m_szTitle).c_str() : "");
        }

        // remove trailing comma
        result.pop_back();

        result += "]";
    } else {
        for (auto& w : g_pCompositor->m_vWorkspaces) {
            const auto PLASTW = w->getLastFocusedWindow();
            result += getFormat("workspace ID %i (%s) on monitor %s:\n\twindows: %i\n\thasfullscreen: %i\n\tlastwindow: 0x%x\n\tlastwindowtitle: %s\n\n", w->m_iID,
                                w->m_szName.c_str(), g_pCompositor->getMonitorFromID(w->m_iMonitorID)->szName.c_str(), g_pCompositor->getWindowsOnWorkspace(w->m_iID),
                                (int)w->m_bHasFullscreenWindow, PLASTW, PLASTW ? PLASTW->m_szTitle.c_str() : "");
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
            result += getFormat(
                R"#("%s": {
    "levels": {
)#",
                escapeJSONStrings(mon->szName).c_str());

            int layerLevel = 0;
            for (auto& level : mon->m_aLayerSurfaceLayers) {
                result += getFormat(
                    R"#(
        "%i": [
)#",
                    layerLevel);
                for (auto& layer : level) {
                    result += getFormat(
                        R"#(                {
                    "address": "0x%x",
                    "x": %i,
                    "y": %i,
                    "w": %i,
                    "h": %i,
                    "namespace": "%s"
                },)#",
                        layer.get(), layer->geometry.x, layer->geometry.y, layer->geometry.width, layer->geometry.height, escapeJSONStrings(layer->szNamespace).c_str());
                }

                // remove trailing comma
                result.pop_back();

                if (level.size() > 0)
                    result += "\n        ";

                result += "],";

                layerLevel++;
            }

            // remove trailing comma
            result.pop_back();

            result += "\n    }\n},";
        }

        // remove trailing comma
        result.pop_back();

        result += "\n}\n";

    } else {
        for (auto& mon : g_pCompositor->m_vMonitors) {
            result += getFormat("Monitor %s:\n", mon->szName.c_str());
            int                                     layerLevel = 0;
            static const std::array<std::string, 4> levelNames = {"background", "bottom", "top", "overlay"};
            for (auto& level : mon->m_aLayerSurfaceLayers) {
                result += getFormat("\tLayer level %i (%s):\n", layerLevel, levelNames[layerLevel].c_str());

                for (auto& layer : level) {
                    result += getFormat("\t\tLayer %x: xywh: %i %i %i %i, namespace: %s\n", layer.get(), layer->geometry.x, layer->geometry.y, layer->geometry.width,
                                        layer->geometry.height, layer->szNamespace.c_str());
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
            result += getFormat(
                R"#(    {
        "address": "0x%x",
        "name": "%s",
        "defaultSpeed": %f
    },)#",
                &m, escapeJSONStrings(m.name).c_str(),
                wlr_input_device_is_libinput(m.mouse) ? libinput_device_config_accel_get_default_speed((libinput_device*)wlr_libinput_get_device_handle(m.mouse)) : 0.f);
        }

        // remove trailing comma
        result.pop_back();
        result += "\n],\n";

        result += "\"keyboards\": [\n";
        for (auto& k : g_pInputManager->m_lKeyboards) {
            const auto KM = g_pInputManager->getActiveLayoutForKeyboard(&k);
            result += getFormat(
                R"#(    {
        "address": "0x%x",
        "name": "%s",
        "rules": "%s",
        "model": "%s",
        "layout": "%s",
        "variant": "%s",
        "options": "%s",
        "active_keymap": "%s",
        "main": %s
    },)#",
                &k, escapeJSONStrings(k.name).c_str(), escapeJSONStrings(k.currentRules.rules).c_str(), escapeJSONStrings(k.currentRules.model).c_str(),
                escapeJSONStrings(k.currentRules.layout).c_str(), escapeJSONStrings(k.currentRules.variant).c_str(), escapeJSONStrings(k.currentRules.options).c_str(),
                escapeJSONStrings(KM).c_str(), (k.active ? "true" : "false"));
        }

        // remove trailing comma
        result.pop_back();
        result += "\n],\n";

        result += "\"tablets\": [\n";

        for (auto& d : g_pInputManager->m_lTabletPads) {
            result += getFormat(
                R"#(    {
        "address": "0x%x",
        "type": "tabletPad",
        "belongsTo": {
            "address": "0x%x",
            "name": "%s"
        }
    },)#",
                &d, d.pTabletParent, escapeJSONStrings(d.pTabletParent ? d.pTabletParent->name : "").c_str());
        }

        for (auto& d : g_pInputManager->m_lTablets) {
            result += getFormat(
                R"#(    {
        "address": "0x%x",
        "name": "%s"
    },)#",
                &d, escapeJSONStrings(d.name).c_str());
        }

        for (auto& d : g_pInputManager->m_lTabletTools) {
            result += getFormat(
                R"#(    {
        "address": "0x%x",
        "type": "tabletTool",
        "belongsTo": "0x%x"
    },)#",
                &d, d.wlrTabletTool ? d.wlrTabletTool->data : 0);
        }

        // remove trailing comma
        result.pop_back();
        result += "\n],\n";

        result += "\"touch\": [\n";

        for (auto& d : g_pInputManager->m_lTouchDevices) {
            result += getFormat(
                R"#(    {
        "address": "0x%x",
        "name": "%s"
    },)#",
                &d, d.name.c_str());
        }

        // remove trailing comma
        if (result[result.size() - 1] == ',')
            result.pop_back();
        result += "\n],\n";

        result += "\"switches\": [\n";

        for (auto& d : g_pInputManager->m_lSwitches) {
            result += getFormat(
                R"#(    {
        "address": "0x%x",
        "name": "%s"
    },)#",
                &d, d.pWlrDevice ? d.pWlrDevice->name : "");
        }

        // remove trailing comma
        if (result[result.size() - 1] == ',')
            result.pop_back();
        result += "\n]\n";

        result += "}\n";

    } else {
        result += "mice:\n";

        for (auto& m : g_pInputManager->m_lMice) {
            result += getFormat(
                "\tMouse at %x:\n\t\t%s\n\t\t\tdefault speed: %f\n", &m, m.name.c_str(),
                (wlr_input_device_is_libinput(m.mouse) ? libinput_device_config_accel_get_default_speed((libinput_device*)wlr_libinput_get_device_handle(m.mouse)) : 0.f));
        }

        result += "\n\nKeyboards:\n";

        for (auto& k : g_pInputManager->m_lKeyboards) {
            const auto KM = g_pInputManager->getActiveLayoutForKeyboard(&k);
            result += getFormat("\tKeyboard at %x:\n\t\t%s\n\t\t\trules: r \"%s\", m \"%s\", l \"%s\", v \"%s\", o \"%s\"\n\t\t\tactive keymap: %s\n\t\t\tmain: %s\n", &k,
                                k.name.c_str(), k.currentRules.rules.c_str(), k.currentRules.model.c_str(), k.currentRules.layout.c_str(), k.currentRules.variant.c_str(),
                                k.currentRules.options.c_str(), KM.c_str(), (k.active ? "yes" : "no"));
        }

        result += "\n\nTablets:\n";

        for (auto& d : g_pInputManager->m_lTabletPads) {
            result += getFormat("\tTablet Pad at %x (belongs to %x -> %s)\n", &d, d.pTabletParent, d.pTabletParent ? d.pTabletParent->name.c_str() : "");
        }

        for (auto& d : g_pInputManager->m_lTablets) {
            result += getFormat("\tTablet at %x:\n\t\t%s\n", &d, d.name.c_str());
        }

        for (auto& d : g_pInputManager->m_lTabletTools) {
            result += getFormat("\tTablet Tool at %x (belongs to %x)\n", &d, d.wlrTabletTool ? d.wlrTabletTool->data : 0);
        }

        result += "\n\nTouch:\n";

        for (auto& d : g_pInputManager->m_lTouchDevices) {
            result += getFormat("\tTouch Device at %x:\n\t\t%s\n", &d, d.name.c_str());
        }

        result += "\n\nSwitches:\n";

        for (auto& d : g_pInputManager->m_lSwitches) {
            result += getFormat("\tSwitch Device at %x:\n\t\t%s\n", &d, d.pWlrDevice ? d.pWlrDevice->name : "");
        }
    }

    return result;
}

std::string animationsRequest(HyprCtl::eHyprCtlOutputFormat format) {
    std::string ret = "";
    if (format == HyprCtl::eHyprCtlOutputFormat::FORMAT_NORMAL) {
        ret += "animations:\n";

        for (auto& ac : g_pConfigManager->getAnimationConfig()) {
            ret += getFormat("\n\tname: %s\n\t\toverriden: %i\n\t\tbezier: %s\n\t\tenabled: %i\n\t\tspeed: %.2f\n\t\tstyle: %s\n", ac.first.c_str(), (int)ac.second.overridden,
                             ac.second.internalBezier.c_str(), ac.second.internalEnabled, ac.second.internalSpeed, ac.second.internalStyle.c_str());
        }

        ret += "beziers:\n";

        for (auto& bz : g_pAnimationManager->getAllBeziers()) {
            ret += getFormat("\n\tname: %s\n", bz.first.c_str());
        }
    } else {
        // json

        ret += "[[";
        for (auto& ac : g_pConfigManager->getAnimationConfig()) {
            ret += getFormat(R"#(
{
    "name": "%s",
    "overridden": %s,
    "bezier": "%s",
    "enabled": %s,
    "speed": %.2f,
    "style": "%s"
},)#",
                             ac.first.c_str(), ac.second.overridden ? "true" : "false", ac.second.internalBezier.c_str(), ac.second.internalEnabled ? "true" : "false",
                             ac.second.internalSpeed, ac.second.internalStyle.c_str());
        }

        ret[ret.length() - 1] = ']';

        ret += ",\n[";

        for (auto& bz : g_pAnimationManager->getAllBeziers()) {
            ret += getFormat(R"#(
{
    "name": "%s"
},)#",
                             bz.first.c_str());
        }

        ret.pop_back();

        ret += "]]";
    }

    return ret;
}

std::string globalShortcutsRequest(HyprCtl::eHyprCtlOutputFormat format) {
    std::string ret       = "";
    const auto  SHORTCUTS = g_pProtocolManager->m_pGlobalShortcutsProtocolManager->getAllShortcuts();
    if (format == HyprCtl::eHyprCtlOutputFormat::FORMAT_NORMAL) {
        for (auto& sh : SHORTCUTS)
            ret += getFormat("%s:%s -> %s\n", sh.appid.c_str(), sh.id.c_str(), sh.description.c_str());
    } else {
        ret += "[";
        for (auto& sh : SHORTCUTS) {
            ret += getFormat(R"#(
{
    "name": "%s",
    "description": "%s"
},)#",
                             escapeJSONStrings(sh.appid + ":" + sh.id).c_str(), escapeJSONStrings(sh.description).c_str());
        }
        ret.pop_back();
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

            ret += getFormat("\n\tmodmask: %u\n\tsubmap: %s\n\tkey: %s\n\tkeycode: %d\n\tdispatcher: %s\n\targ: %s\n\n", kb.modmask, kb.submap.c_str(), kb.key.c_str(), kb.keycode,
                             kb.handler.c_str(), kb.arg.c_str());
        }
    } else {
        // json
        ret += "[";
        for (auto& kb : g_pKeybindManager->m_lKeybinds) {
            ret += getFormat(
                R"#(
{
    "locked": %s,
    "mouse": %s,
    "release": %s,
    "repeat": %s,
    "modmask": %u,
    "submap": "%s",
    "key": "%s",
    "keycode": %i,
    "dispatcher": "%s",
    "arg": "%s"
},)#",
                kb.locked ? "true" : "false", kb.mouse ? "true" : "false", kb.release ? "true" : "false", kb.repeat ? "true" : "false", kb.modmask,
                escapeJSONStrings(kb.submap).c_str(), escapeJSONStrings(kb.key).c_str(), kb.keycode, escapeJSONStrings(kb.handler).c_str(), escapeJSONStrings(kb.arg).c_str());
        }
        ret.pop_back();
        ret += "]";
    }

    return ret;
}

std::string versionRequest(HyprCtl::eHyprCtlOutputFormat format) {

    if (format == HyprCtl::eHyprCtlOutputFormat::FORMAT_NORMAL) {
        std::string result = "Hyprland, built from branch " + std::string(GIT_BRANCH) + " at commit " + GIT_COMMIT_HASH + GIT_DIRTY + " (" +
            removeBeginEndSpacesTabs(GIT_COMMIT_MESSAGE).c_str() + ").\nflags: (if any)\n";

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
        std::string result = getFormat(
            R"#({
    "branch": "%s",
    "commit": "%s",
    "dirty": %s,
    "commit_message": "%s",
    "flags": [)#",
            GIT_BRANCH, GIT_COMMIT_HASH, (strcmp(GIT_DIRTY, "dirty") == 0 ? "true" : "false"), removeBeginEndSpacesTabs(GIT_COMMIT_MESSAGE).c_str());

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

        if (result[result.length() - 1] == ',')
            result.pop_back();

        result += "]\n}";

        return result;
    }

    return ""; // make the compiler happy
}

std::string dispatchRequest(std::string in) {
    // get rid of the dispatch keyword
    in = in.substr(in.find_first_of(' ') + 1);

    const auto DISPATCHSTR = in.substr(0, in.find_first_of(' '));

    const auto DISPATCHARG = in.substr(in.find_first_of(' ') + 1);

    const auto DISPATCHER = g_pKeybindManager->m_mDispatchers.find(DISPATCHSTR);
    if (DISPATCHER == g_pKeybindManager->m_mDispatchers.end())
        return "Invalid dispatcher";

    DISPATCHER->second(DISPATCHARG);

    Debug::log(LOG, "Hyprctl: dispatcher %s : %s", DISPATCHSTR.c_str(), DISPATCHARG.c_str());

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
    if (COMMAND.contains("decoration:") || COMMAND.contains("border")) {
        for (auto& m : g_pCompositor->m_vMonitors) {
            g_pHyprRenderer->damageMonitor(m.get());
            g_pLayoutManager->getCurrentLayout()->recalculateMonitor(m->ID);
        }
    }

    Debug::log(LOG, "Hyprctl: keyword %s : %s", COMMAND.c_str(), VALUE.c_str());

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
        return getFormat("%i, %i", (int)CURSORPOS.x, (int)CURSORPOS.y);
    } else {
        return getFormat(R"#(
{
    "x": %i,
    "y": %i
}
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

    const auto THEME = curitem;

    nextItem();

    const auto SIZE = curitem;

    if (!isNumber(SIZE)) {
        return "size not int";
    }

    const auto SIZEINT = std::stoi(SIZE);

    if (SIZEINT < 1) {
        return "size must be positive";
    }

    wlr_xcursor_manager_destroy(g_pCompositor->m_sWLRXCursorMgr);

    g_pCompositor->m_sWLRXCursorMgr = wlr_xcursor_manager_create(THEME.c_str(), SIZEINT);

    setenv("XCURSOR_SIZE", SIZE.c_str(), true);
    setenv("XCURSOR_THEME", THEME.c_str(), true);

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
        if (vars[4].find("lock") == 0) {
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
        } else {
            return "prop not found";
        }
    } catch (std::exception& e) { return "error in parsing prop value: " + std::string(e.what()); }

    g_pCompositor->updateAllWindowsAnimatedDecorationValues();

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
        return getFormat("option %s\n\tint: %lld\n\tfloat: %f\n\tstr: \"%s\"\n\tdata: %x", curitem.c_str(), PCFGOPT->intValue, PCFGOPT->floatValue, PCFGOPT->strValue.c_str(),
                         PCFGOPT->data.get());
    else {
        return getFormat(
            R"#(
{
    "option": "%s",
    "int": %lld,
    "float": %f,
    "str": "%s",
    "data": "0x%x"
}
)#",
            curitem.c_str(), PCFGOPT->intValue, PCFGOPT->floatValue, PCFGOPT->strValue.c_str(), PCFGOPT->data.get());
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
            list += getFormat("\nPlugin %s by %s:\n\tHandle: %lx\n\tVersion: %s\n\tDescription: %s\n", p->name.c_str(), p->author.c_str(), p->m_pHandle, p->version.c_str(),
                              p->description.c_str());
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
    else if (request.find("reload") == 0)
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
    else if (request.find("plugin") == 0)
        return dispatchPlugin(request);
    else if (request.find("notify") == 0)
        return dispatchNotify(request);
    else if (request.find("setprop") == 0)
        return dispatchSetProp(request);
    else if (request.find("seterror") == 0)
        return dispatchSeterror(request);
    else if (request.find("switchxkblayout") == 0)
        return switchXKBLayoutRequest(request);
    else if (request.find("output") == 0)
        return dispatchOutput(request);
    else if (request.find("dispatch") == 0)
        return dispatchRequest(request);
    else if (request.find("keyword") == 0)
        return dispatchKeyword(request);
    else if (request.find("setcursor") == 0)
        return dispatchSetCursor(request);
    else if (request.find("getoption") == 0)
        return dispatchGetOption(request, format);
    else if (request.find("[[BATCH]]") == 0)
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

    const auto  ACCEPTEDCONNECTION = accept(HyprCtl::iSocketFD, (sockaddr*)&clientAddress, &clientSize);

    char        readBuffer[1024];

    auto        messageSize                              = read(ACCEPTEDCONNECTION, readBuffer, 1024);
    readBuffer[messageSize == 1024 ? 1023 : messageSize] = '\0';

    std::string request(readBuffer);

    std::string reply = "";

    try {
        reply = getReply(request);
    } catch (std::exception& e) {
        Debug::log(ERR, "Error in request: %s", e.what());
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

    iSocketFD = socket(AF_UNIX, SOCK_STREAM, 0);

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

    Debug::log(LOG, "Hypr socket started at %s", socketPath.c_str());

    wl_event_loop_add_fd(g_pCompositor->m_sWLEventLoop, iSocketFD, WL_EVENT_READABLE, hyprCtlFDTick, nullptr);
}
