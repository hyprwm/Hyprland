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

#include <string>

std::string monitorsRequest(HyprCtl::eHyprCtlOutputFormat format) {
    std::string result = "";
    if (format == HyprCtl::FORMAT_JSON) {
        result += "[";
        
        for (auto& m : g_pCompositor->m_vMonitors) {
            result += getFormat(
R"#({
    "id": %i,
    "name": "%s",
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
    "active": %s
},)#",
                m->ID,
                escapeJSONStrings(m->szName).c_str(),
                (int)m->vecPixelSize.x, (int)m->vecPixelSize.y,
                m->refreshRate,
                (int)m->vecPosition.x, (int)m->vecPosition.y,
                m->activeWorkspace, escapeJSONStrings(g_pCompositor->getWorkspaceByID(m->activeWorkspace)->m_szName).c_str(),
                (int)m->vecReservedTopLeft.x, (int)m->vecReservedTopLeft.y, (int)m->vecReservedBottomRight.x, (int)m->vecReservedBottomRight.y,
                m->scale,
                (int)m->transform,
                (m.get() == g_pCompositor->m_pLastMonitor ? "true" : "false")
            );
        }

        // remove trailing comma
        result.pop_back();

        result += "]";
    } else {
        for (auto& m : g_pCompositor->m_vMonitors) {
            result += getFormat("Monitor %s (ID %i):\n\t%ix%i@%f at %ix%i\n\tactive workspace: %i (%s)\n\treserved: %i %i %i %i\n\tscale: %.2f\n\ttransform: %i\n\tactive: %s\n\n",
                                m->szName.c_str(), m->ID, (int)m->vecPixelSize.x, (int)m->vecPixelSize.y, m->refreshRate, (int)m->vecPosition.x, (int)m->vecPosition.y, m->activeWorkspace, g_pCompositor->getWorkspaceByID(m->activeWorkspace)->m_szName.c_str(), (int)m->vecReservedTopLeft.x, (int)m->vecReservedTopLeft.y, (int)m->vecReservedBottomRight.x, (int)m->vecReservedBottomRight.y, m->scale, (int)m->transform, (m.get() == g_pCompositor->m_pLastMonitor ? "yes" : "no"));
        }
    }

    return result;
}

std::string clientsRequest(HyprCtl::eHyprCtlOutputFormat format) {
    std::string result = "";
    if (format == HyprCtl::FORMAT_JSON) {
        result += "[";

        for (auto& w : g_pCompositor->m_vWindows) {
            if (w->m_bIsMapped) {
                result += getFormat(
R"#({
    "address": "0x%x",
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
    "pid": %i,
    "xwayland": %s
},)#",
                    w.get(),
                    (int)w->m_vRealPosition.vec().x, (int)w->m_vRealPosition.vec().y,
                    (int)w->m_vRealSize.vec().x, (int)w->m_vRealSize.vec().y,
                    w->m_iWorkspaceID, escapeJSONStrings(w->m_iWorkspaceID == -1 ? "" : g_pCompositor->getWorkspaceByID(w->m_iWorkspaceID) ? g_pCompositor->getWorkspaceByID(w->m_iWorkspaceID)->m_szName : std::string("Invalid workspace " + std::to_string(w->m_iWorkspaceID))).c_str(),
                    ((int)w->m_bIsFloating == 1 ? "true" : "false"),
                    w->m_iMonitorID,
                    escapeJSONStrings(g_pXWaylandManager->getAppIDClass(w.get())).c_str(),
                    escapeJSONStrings(g_pXWaylandManager->getTitle(w.get())).c_str(),
                    w->getPID(),
                    ((int)w->m_bIsX11 == 1 ? "true" : "false")
                );
            }
        }

        // remove trailing comma
        if (result != "[")
          result.pop_back();

        result += "]";
    } else {
        for (auto& w : g_pCompositor->m_vWindows) {
            if (w->m_bIsMapped) {
                result += getFormat("Window %x -> %s:\n\tat: %i,%i\n\tsize: %i,%i\n\tworkspace: %i (%s)\n\tfloating: %i\n\tmonitor: %i\n\tclass: %s\n\ttitle: %s\n\tpid: %i\n\txwayland: %i\n\n",
                                w.get(), w->m_szTitle.c_str(), (int)w->m_vRealPosition.vec().x, (int)w->m_vRealPosition.vec().y, (int)w->m_vRealSize.vec().x, (int)w->m_vRealSize.vec().y, w->m_iWorkspaceID, (w->m_iWorkspaceID == -1 ? "" : g_pCompositor->getWorkspaceByID(w->m_iWorkspaceID) ? g_pCompositor->getWorkspaceByID(w->m_iWorkspaceID)->m_szName.c_str() : std::string("Invalid workspace " + std::to_string(w->m_iWorkspaceID)).c_str()), (int)w->m_bIsFloating, w->m_iMonitorID, g_pXWaylandManager->getAppIDClass(w.get()).c_str(), g_pXWaylandManager->getTitle(w.get()).c_str(), w->getPID(), (int)w->m_bIsX11);
        
            }
        }
    }
    return result;
}

std::string workspacesRequest(HyprCtl::eHyprCtlOutputFormat format) {
    std::string result = "";
    if (format == HyprCtl::FORMAT_JSON) {
        result += "[";

        for (auto& w : g_pCompositor->m_vWorkspaces) {
            result += getFormat(
R"#({
    "id": %i,
    "name": "%s",
    "monitor": "%s",
    "windows": %i,
    "hasfullscreen": %s
},)#",
                w->m_iID,
                escapeJSONStrings(w->m_szName).c_str(),
                escapeJSONStrings(g_pCompositor->getMonitorFromID(w->m_iMonitorID)->szName).c_str(),
                g_pCompositor->getWindowsOnWorkspace(w->m_iID),
                ((int)w->m_bHasFullscreenWindow == 1 ? "true" : "false")
            );
        }

        // remove trailing comma
        result.pop_back();

        result += "]";
    } else {
        for (auto& w : g_pCompositor->m_vWorkspaces) {
            result += getFormat("workspace ID %i (%s) on monitor %s:\n\twindows: %i\n\thasfullscreen: %i\n\n",
                                w->m_iID, w->m_szName.c_str(), g_pCompositor->getMonitorFromID(w->m_iMonitorID)->szName.c_str(), g_pCompositor->getWindowsOnWorkspace(w->m_iID), (int)w->m_bHasFullscreenWindow);
        }
    }
    return result;
}

std::string activeWindowRequest(HyprCtl::eHyprCtlOutputFormat format) {
    const auto PWINDOW = g_pCompositor->m_pLastWindow;

    if (!g_pCompositor->windowValidMapped(PWINDOW))
        return format == HyprCtl::FORMAT_JSON ? "{}" : "Invalid";

    if (format == HyprCtl::FORMAT_JSON) {
        return getFormat(
R"#({
    "address": "0x%x",
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
    "pid": %i,
    "xwayland": %s
})#",
            PWINDOW,
            (int)PWINDOW->m_vRealPosition.vec().x, (int)PWINDOW->m_vRealPosition.vec().y,
            (int)PWINDOW->m_vRealSize.vec().x, (int)PWINDOW->m_vRealSize.vec().y,
            PWINDOW->m_iWorkspaceID, escapeJSONStrings(PWINDOW->m_iWorkspaceID == -1 ? "" : g_pCompositor->getWorkspaceByID(PWINDOW->m_iWorkspaceID)->m_szName).c_str(),
            ((int)PWINDOW->m_bIsFloating == 1 ? "true" : "false"),
            PWINDOW->m_iMonitorID,
            escapeJSONStrings(g_pXWaylandManager->getAppIDClass(PWINDOW)).c_str(),
            escapeJSONStrings(g_pXWaylandManager->getTitle(PWINDOW)).c_str(),
            PWINDOW->getPID(),
            ((int)PWINDOW->m_bIsX11 == 1 ? "true" : "false")
        );
    } else {
        return getFormat("Window %x -> %s:\n\tat: %i,%i\n\tsize: %i,%i\n\tworkspace: %i (%s)\n\tfloating: %i\n\tmonitor: %i\n\tclass: %s\n\ttitle: %s\n\tpid: %i\n\txwayland: %i\n\n",
                            PWINDOW, PWINDOW->m_szTitle.c_str(), (int)PWINDOW->m_vRealPosition.vec().x, (int)PWINDOW->m_vRealPosition.vec().y, (int)PWINDOW->m_vRealSize.vec().x, (int)PWINDOW->m_vRealSize.vec().y, PWINDOW->m_iWorkspaceID, (PWINDOW->m_iWorkspaceID == -1 ? "" : g_pCompositor->getWorkspaceByID(PWINDOW->m_iWorkspaceID)->m_szName.c_str()), (int)PWINDOW->m_bIsFloating, (int)PWINDOW->m_iMonitorID, g_pXWaylandManager->getAppIDClass(PWINDOW).c_str(), g_pXWaylandManager->getTitle(PWINDOW).c_str(), PWINDOW->getPID(), (int)PWINDOW->m_bIsX11);
    }
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
                escapeJSONStrings(mon->szName).c_str()
            ); 

            int layerLevel = 0;
            for (auto& level : mon->m_aLayerSurfaceLists) {
                result += getFormat(
R"#(        
        "%i": [
)#",
                    layerLevel
                );
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
					layer.get(),
                    layer->geometry.x,
                    layer->geometry.y,
                    layer->geometry.width,
                    layer->geometry.height,
                    escapeJSONStrings(layer->szNamespace).c_str()
                    );
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
            int layerLevel = 0;
            for (auto& level : mon->m_aLayerSurfaceLists) {
                result += getFormat("\tLayer level %i:\n", layerLevel);

                for (auto& layer : level) {
                    result += getFormat("\t\tLayer %x: xywh: %i %i %i %i, namespace: %s\n", layer.get(), layer->geometry.x, layer->geometry.y, layer->geometry.width, layer->geometry.height, layer->szNamespace.c_str());
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
        "name": "%s"
    },)#",
                &m,
                escapeJSONStrings(m.mouse->name).c_str()
            );
        }

        // remove trailing comma
        result.pop_back();
        result += "\n],\n";

        result += "\"keyboards\": [\n";
        for (auto& k : g_pInputManager->m_lKeyboards) {
            const auto KM = xkb_keymap_layout_get_name(wlr_keyboard_from_input_device(k.keyboard)->keymap, 0);
            result += getFormat(
R"#(    {
        "address": "0x%x",
        "name": "%s",
        "rules": "%s",
        "model": "%s",
        "layout": "%s",
        "variant": "%s",
        "options": "%s",
        "active_keymap": "%s"
    },)#",
                &k,
                escapeJSONStrings(k.keyboard->name).c_str(),
                escapeJSONStrings(k.currentRules.rules).c_str(),
                escapeJSONStrings(k.currentRules.model).c_str(),
                escapeJSONStrings(k.currentRules.layout).c_str(),
                escapeJSONStrings(k.currentRules.variant).c_str(),
                escapeJSONStrings(k.currentRules.options).c_str(),
                escapeJSONStrings(KM).c_str()
            );
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
                &d,
                d.pTabletParent,
                escapeJSONStrings(d.pTabletParent ? d.pTabletParent->wlrDevice ? d.pTabletParent->wlrDevice->name : "" : "").c_str()
            );
        }

        for (auto& d : g_pInputManager->m_lTablets) {
            result += getFormat(
R"#(    {
        "address": "0x%x",
        "name": "%s"
    },)#",
                &d,
                escapeJSONStrings(d.wlrDevice ? d.wlrDevice->name : "").c_str()
            );
        }

        for (auto& d : g_pInputManager->m_lTabletTools) {
            result += getFormat(
R"#(    {
        "address": "0x%x",
        "type": "tabletTool",
        "belongsTo": "0x%x"
    },)#",
                &d,
                d.wlrTabletTool ? d.wlrTabletTool->data : 0
            );
        }

        // remove trailing comma
        if (result[result.size() - 1] == ',')
            result.pop_back();
        result += "\n]\n";

        result += "}\n";

    } else {
        result += "mice:\n";

        for (auto& m : g_pInputManager->m_lMice) {
            result += getFormat("\tMouse at %x:\n\t\t%s\n", &m, m.mouse->name);
        }

        result += "\n\nKeyboards:\n";

        for (auto& k : g_pInputManager->m_lKeyboards) {
            const auto KM = xkb_keymap_layout_get_name(wlr_keyboard_from_input_device(k.keyboard)->keymap, 0);
            result += getFormat("\tKeyboard at %x:\n\t\t%s\n\t\t\trules: r \"%s\", m \"%s\", l \"%s\", v \"%s\", o \"%s\"\n\t\t\tactive keymap: %s\n", &k, k.keyboard->name, k.currentRules.rules.c_str(), k.currentRules.model.c_str(), k.currentRules.layout.c_str(), k.currentRules.variant.c_str(), k.currentRules.options.c_str(), KM);
        }

        result += "\n\nTablets:\n";

        for (auto& d : g_pInputManager->m_lTabletPads) {
            result += getFormat("\tTablet Pad at %x (belongs to %x -> %s)\n", &d, d.pTabletParent, d.pTabletParent ? d.pTabletParent->wlrDevice ? d.pTabletParent->wlrDevice->name : "" : "");
        }

        for (auto& d : g_pInputManager->m_lTablets) {
            result += getFormat("\tTablet at %x:\n\t\t%s\n", &d, d.wlrDevice ? d.wlrDevice->name : "");
        }

        for (auto& d : g_pInputManager->m_lTabletTools) {
            result += getFormat("\tTablet Tool at %x (belongs to %x)\n", &d, d.wlrTabletTool ? d.wlrTabletTool->data : 0);
        }
    }

    return result;
}

std::string versionRequest() {
    std::string result = "Hyprland, built from branch " + std::string(GIT_BRANCH) + " at commit " + GIT_COMMIT_HASH + GIT_DIRTY + " (" + GIT_COMMIT_MESSAGE + ").\nflags: (if any)\n";

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

    const auto COMMAND = in.substr(0, in.find_first_of(' '));

    const auto VALUE = in.substr(in.find_first_of(' ') + 1);

    std::string retval = g_pConfigManager->parseKeyword(COMMAND, VALUE, true);

    if (COMMAND == "monitor")
        g_pConfigManager->m_bWantsMonitorReload = true; // for monitor keywords

    if (COMMAND.contains("input"))
        g_pInputManager->setKeyboardLayout(); // update kb layout

    if (COMMAND.contains("general:layout"))
        g_pLayoutManager->switchToLayout(g_pConfigManager->getString("general:layout"));  // update layout

    Debug::log(LOG, "Hyprctl: keyword %s : %s", COMMAND.c_str(), VALUE.c_str());

    if (retval == "") 
        return "ok";

    return retval;
}

std::string reloadRequest() {
    g_pConfigManager->m_bForceReload = true;

    return "ok";
}

std::string killRequest() {
    g_pInputManager->setClickMode(CLICKMODE_KILL);

    return "ok";
}

std::string splashRequest() {
    return g_pCompositor->m_szCurrentSplash;
}

std::string getReply(std::string);

std::string dispatchBatch(std::string request) {
    // split by ;

    request = request.substr(9);
    std::string curitem = "";
    std::string reply = "";

    auto nextItem = [&]() {
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
        return versionRequest();
    else if (request == "reload")
        return reloadRequest();
    else if (request == "devices")
        return devicesRequest(format);
    else if (request == "splash")
        return splashRequest();
    else if (request.find("dispatch") == 0)
        return dispatchRequest(request);
    else if (request.find("keyword") == 0)
        return dispatchKeyword(request);
    else if (request.find("[[BATCH]]") == 0)
        return dispatchBatch(request);

    return "unknown request";
}

void HyprCtl::tickHyprCtl() {
    if (!requestMade)
        return;

    std::string reply = "";

    try {
        reply = getReply(request);
    } catch (std::exception& e) {
        Debug::log(ERR, "Error in request: %s", e.what());
        reply = "Err: " + std::string(e.what());
    }

    request = reply;

    requestMade = false;
    requestReady = true;

    if (g_pConfigManager->m_bWantsMonitorReload) {
        g_pConfigManager->ensureDPMS();
    }
}

std::string getRequestFromThread(std::string rq) {
    
    while (HyprCtl::request != "" || HyprCtl::requestMade || HyprCtl::requestReady) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    HyprCtl::request = rq;
    HyprCtl::requestMade = true;

    while (!HyprCtl::requestReady) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    HyprCtl::requestReady = false;
    HyprCtl::requestMade = false;

    std::string toReturn = HyprCtl::request;

    HyprCtl::request = "";

    return toReturn;
}

void HyprCtl::startHyprCtlSocket() {
    std::thread([&]() {
        const auto SOCKET = socket(AF_UNIX, SOCK_STREAM, 0);

        if (SOCKET < 0) {
            Debug::log(ERR, "Couldn't start the Hyprland Socket. (1) IPC will not work.");
            return;
        }

        sockaddr_un SERVERADDRESS = {.sun_family = AF_UNIX};

        std::string socketPath = "/tmp/hypr/" + g_pCompositor->m_szInstanceSignature + "/.socket.sock";

        strcpy(SERVERADDRESS.sun_path, socketPath.c_str());

        bind(SOCKET, (sockaddr*)&SERVERADDRESS, SUN_LEN(&SERVERADDRESS));

        // 10 max queued.
        listen(SOCKET, 10);

        sockaddr_in clientAddress;
        socklen_t clientSize = sizeof(clientAddress);

        char readBuffer[1024] = {0};

        Debug::log(LOG, "Hypr socket started at %s", socketPath.c_str());

        while(1) {
            const auto ACCEPTEDCONNECTION = accept(SOCKET, (sockaddr*)&clientAddress, &clientSize);

            if (ACCEPTEDCONNECTION < 0) {
                Debug::log(ERR, "Couldn't listen on the Hyprland Socket. (3) IPC will not work.");
                break;
            }

            auto messageSize = read(ACCEPTEDCONNECTION, readBuffer, 1024);
            readBuffer[messageSize == 1024 ? 1023 : messageSize] = '\0';

            std::string request(readBuffer);

            std::string reply = getRequestFromThread(request);
            
            write(ACCEPTEDCONNECTION, reply.c_str(), reply.length());

            close(ACCEPTEDCONNECTION);
        }

        close(SOCKET);
    }).detach();
}
