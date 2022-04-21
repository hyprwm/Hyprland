#include "HyprCtl.hpp"

#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#include <string>

std::string monitorsRequest() {
    std::string result = "";
    for (auto& m : g_pCompositor->m_lMonitors) {
        result += getFormat("Monitor %s (ID %i):\n\t%ix%i@%f at %ix%i\n\tactive workspace: %i (%s)\n\treserved: %i %i %i %i\n\n",
                            m.szName.c_str(), m.ID, (int)m.vecSize.x, (int)m.vecSize.y, m.refreshRate, (int)m.vecPosition.x, (int)m.vecPosition.y, m.activeWorkspace, g_pCompositor->getWorkspaceByID(m.activeWorkspace)->m_szName.c_str(), (int)m.vecReservedTopLeft.x, (int)m.vecReservedTopLeft.y, (int)m.vecReservedBottomRight.x, (int)m.vecReservedBottomRight.y);
    }

    return result;
}

std::string clientsRequest() {
    std::string result = "";
    for (auto& w : g_pCompositor->m_lWindows) {
        result += getFormat("Window %x -> %s:\n\tat: %i,%i\n\tsize: %i, %i\n\tworkspace: %i (%s)\n\tfloating: %i\n\n",
                            &w, w.m_szTitle.c_str(), (int)w.m_vRealPosition.x, (int)w.m_vRealPosition.y, (int)w.m_vRealSize.x, (int)w.m_vRealSize.y, w.m_iWorkspaceID, (w.m_iWorkspaceID == -1 ? "" : g_pCompositor->getWorkspaceByID(w.m_iWorkspaceID)->m_szName.c_str()), (int)w.m_bIsFloating);
    }
    return result;
}

std::string workspacesRequest() {
    std::string result = "";
    for (auto& w : g_pCompositor->m_lWorkspaces) {
        result += getFormat("workspace ID %i (%s) on monitor %s:\n\twindows: %i\n\thasfullscreen: %i\n\n",
                            w.m_iID, w.m_szName.c_str(), g_pCompositor->getMonitorFromID(w.m_iMonitorID)->szName.c_str(), g_pCompositor->getWindowsOnWorkspace(w.m_iID), (int)w.m_bHasFullscreenWindow);
    }
    return result;
}

std::string activeWindowRequest() {
    const auto PWINDOW = g_pCompositor->m_pLastWindow;

    if (!g_pCompositor->windowValidMapped(PWINDOW))
        return "Invalid";

    return getFormat("Window %x -> %s:\n\tat: %i,%i\n\tsize: %i, %i\n\tworkspace: %i (%s)\n\tfloating: %i\n\n",
                        PWINDOW, PWINDOW->m_szTitle.c_str(), (int)PWINDOW->m_vRealPosition.x, (int)PWINDOW->m_vRealPosition.y, (int)PWINDOW->m_vRealSize.x, (int)PWINDOW->m_vRealSize.y, PWINDOW->m_iWorkspaceID, (PWINDOW->m_iWorkspaceID == -1 ? "" : g_pCompositor->getWorkspaceByID(PWINDOW->m_iWorkspaceID)->m_szName.c_str()), (int)PWINDOW->m_bIsFloating);
}

std::string layersRequest() {
    std::string result = "";

    for (auto& mon : g_pCompositor->m_lMonitors) {
        result += getFormat("Monitor %s:\n");
        int layerLevel = 0;
        for (auto& level : mon.m_aLayerSurfaceLists) {
            result += getFormat("\tLayer level %i:\n", layerLevel);

            for (auto& layer : level) {
                result += getFormat("\t\tLayer %x: xywh: %i %i %i %i\n", layer, layer->geometry.x, layer->geometry.y, layer->geometry.width, layer->geometry.height);
            }

            layerLevel++;
        }
        result += "\n\n";
    }

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

    Debug::log(LOG, "Hyprctl: keyword %s : %s", COMMAND.c_str(), VALUE.c_str());

    if (retval == "") 
        return "ok";

    return retval;
}

void HyprCtl::tickHyprCtl() {
    if (!requestMade)
        return;

    std::string reply = "";

    try {
        if (request == "monitors")
            reply = monitorsRequest();
        else if (request == "workspaces")
            reply = workspacesRequest();
        else if (request == "clients")
            reply = clientsRequest();
        else if (request == "activewindow")
            reply = activeWindowRequest();
        else if (request == "layers")
            reply = layersRequest();
        else if (request.find("dispatch") == 0)
            reply = dispatchRequest(request);
        else if (request.find("keyword") == 0)
            reply = dispatchKeyword(request);
    } catch (std::exception& e) {
        Debug::log(ERR, "Error in request: %s", e.what());
        reply = "Err: " + std::string(e.what());
    }

    request = reply;

    requestMade = false;
    requestReady = true;
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
        uint16_t connectPort = 9187;

        const auto SOCKET = socket(AF_INET, SOCK_STREAM, 0);

        if (SOCKET < 0) {
            Debug::log(ERR, "Couldn't start the Hyprland Socket. (1) IPC will not work.");
            return;
        }

        sockaddr_in SERVERADDRESS = {.sin_family = AF_INET, .sin_port = connectPort, .sin_addr = (in_addr)INADDR_ANY};

        while(connectPort < 11000) {
            if (bind(SOCKET, (sockaddr*)&SERVERADDRESS, sizeof(SERVERADDRESS)) < 0) {
                Debug::log(LOG, "IPC: Port %d failed with an error: %s", connectPort, strerror(errno));
            } else {
                break;
            }

            connectPort++;
            SERVERADDRESS.sin_port = connectPort;
        }
        

        // 10 max queued.
        listen(SOCKET, 10);

        sockaddr_in clientAddress;
        socklen_t clientSize = sizeof(clientAddress);

        char readBuffer[1024] = {0};

        Debug::log(LOG, "Hypr socket started on port %i", connectPort);

        std::string cmd = "rm -f /tmp/hypr/.socket";
        system(cmd.c_str()); // forgive me for using system() but it works and it doesnt matter here that much
        cmd = "echo \"" + std::to_string(connectPort) + "\" > /tmp/hypr/.socket";
        system(cmd.c_str());

        while(1) {
            const auto ACCEPTEDCONNECTION = accept(SOCKET, (sockaddr*)&clientAddress, &clientSize);

            if (ACCEPTEDCONNECTION < 0) {
                Debug::log(ERR, "Couldn't listen on the Hyprland Socket. (3) IPC will not work.");
                break;
            }

            auto messageSize = read(ACCEPTEDCONNECTION, readBuffer, 1024);
            readBuffer[messageSize == 1024 ? 1024 : messageSize] = '\0';

            std::string request(readBuffer);

            std::string reply = getRequestFromThread(request);
            
            write(ACCEPTEDCONNECTION, reply.c_str(), reply.length());

            close(ACCEPTEDCONNECTION);
        }

        close(SOCKET);
    }).detach();
}