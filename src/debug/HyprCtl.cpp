#include "HyprCtl.hpp"

#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>

std::string getFormat(const char* fmt, ...) {
    char buf[2048] = "";

    va_list args;
    va_start(args, fmt);

    vsprintf(buf, fmt , args);

    va_end(args);

    return std::string(buf);
}

std::string monitorsRequest() {
    std::string result = "";
    for (auto& m : g_pCompositor->m_lMonitors) {
        result += getFormat("Monitor %s (ID %i):\n\t%ix%i@%f at %ix%i\n\tactive workspace: %i\n\treserved: %i %i %i %i\n\n",
                            m.szName.c_str(), m.ID, (int)m.vecSize.x, (int)m.vecSize.y, m.refreshRate, (int)m.vecPosition.x, (int)m.vecPosition.y, m.activeWorkspace, (int)m.vecReservedTopLeft.x, (int)m.vecReservedTopLeft.y, (int)m.vecReservedBottomRight.x, (int)m.vecReservedBottomRight.y);
    }

    return result;
}

std::string clientsRequest() {
    std::string result = "";
    for (auto& w : g_pCompositor->m_lWindows) {
        result += getFormat("Window %x -> %s:\n\tat: %i,%i\n\tsize: %i, %i\n\tworkspace: %i\n\tfloating: %i\n\n",
                            &w, w.m_szTitle.c_str(), (int)w.m_vRealPosition.x, (int)w.m_vRealPosition.y, (int)w.m_vRealSize.x, (int)w.m_vRealSize.y, w.m_iWorkspaceID, (int)w.m_bIsFloating);
    }
    return result;
}

std::string workspacesRequest() {
    std::string result = "";
    for (auto& w : g_pCompositor->m_lWorkspaces) {
        result += getFormat("workspace ID %i on monitor %s:\n\twindows: %i\n\thasfullscreen: %i\n\n",
                            w.ID, g_pCompositor->getMonitorFromID(w.monitorID)->szName.c_str(), g_pCompositor->getWindowsOnWorkspace(w.ID), (int)w.hasFullscreenWindow);
    }
    return result;
}

std::string activeWindowRequest() {
    const auto PWINDOW = g_pCompositor->getWindowFromSurface(g_pCompositor->m_pLastFocus);

    if (!g_pCompositor->windowValidMapped(PWINDOW))
        return "Invalid";

    return getFormat("Window %x -> %s:\n\tat: %i,%i\n\tsize: %i, %i\n\tworkspace: %i\n\tfloating: %i\n\n",
                        PWINDOW, PWINDOW->m_szTitle.c_str(), (int)PWINDOW->m_vRealPosition.x, (int)PWINDOW->m_vRealPosition.y, (int)PWINDOW->m_vRealSize.x, (int)PWINDOW->m_vRealSize.y, PWINDOW->m_iWorkspaceID, (int)PWINDOW->m_bIsFloating);
}

std::string layersRequest() {
    std::string result = "";

    for (auto& mon : g_pCompositor->m_lMonitors) {
        result += getFormat("Monitor %s:\n");
        int layerLevel = 0;
        for (auto& level : mon.m_aLayerSurfaceLists) {
            result += getFormat("\tLayer level %i:\n", layerLevel);

            for (auto& layer : level) {
                result += getFormat("\t\tLayer %x -> %s: xywh: %i %i %i %i\n", layer, (layer->layerSurface ? layer->layerSurface->_namespace : "null"), layer->geometry.x, layer->geometry.y, layer->geometry.width, layer->geometry.height);
            }

            layerLevel++;
        }
        result += "\n\n";
    }

    return result;
}

void HyprCtl::startHyprCtlSocket() {
    int port = 9187;

    std::thread([&]() {
        const auto SOCKET = socket(AF_INET, SOCK_STREAM, 0);

        if (SOCKET < 0) {
            Debug::log(ERR, "Couldn't start the Hyprland Socket. (1) IPC will not work.");
            return;
        }

        sockaddr_in SERVERADDRESS = {.sin_family = AF_INET, .sin_port = port, .sin_addr = (in_addr)INADDR_ANY};

        bool bound = false;

        while (port++ < 9200) {
            if (bind(SOCKET, (sockaddr*)&SERVERADDRESS, sizeof(SERVERADDRESS)) >= 0) {
                bound = true;
                break;
            }

            SERVERADDRESS = {.sin_family = AF_INET, .sin_port = port, .sin_addr = (in_addr)INADDR_ANY};
        }

        if (!bound) {
            Debug::log(ERR, "Couldn't start the Hyprland Socket. (2) IPC will not work.");
            return;
        }

        // 10 max queued.
        listen(SOCKET, 10);

        sockaddr_in clientAddress;
        socklen_t clientSize = sizeof(clientAddress);

        char readBuffer[1024] = {0};

        Debug::log(LOG, "Hypr socket started on port %i", SERVERADDRESS.sin_port);

        std::string cmd = "rm -f /tmp/hypr/.socket && echo \"" + std::to_string(SERVERADDRESS.sin_port) + "\" > /tmp/hypr/.socket";
        system(cmd.c_str()); // forgive me for using system() but it works and it doesnt matter here that much

        while(1) {
            const auto ACCEPTEDCONNECTION = accept(SOCKET, (sockaddr*)&clientAddress, &clientSize);

            if (ACCEPTEDCONNECTION < 0) {
                Debug::log(ERR, "Couldn't listen on the Hyprland Socket. (3) IPC will not work.");
                break;
            }

            auto messageSize = read(ACCEPTEDCONNECTION, readBuffer, 1024);
            readBuffer[messageSize == 1024 ? 1024 : messageSize] = '\0';

            std::string request(readBuffer);

            std::string reply = "";
            if (request == "monitors") reply = monitorsRequest();
            if (request == "workspaces") reply = workspacesRequest();
            if (request == "clients") reply = clientsRequest();
            if (request == "activewindow") reply = activeWindowRequest();
            if (request == "layers") reply = layersRequest();

            write(ACCEPTEDCONNECTION, reply.c_str(), reply.length());

            close(ACCEPTEDCONNECTION);
        }

        close(SOCKET);
    }).detach();
}