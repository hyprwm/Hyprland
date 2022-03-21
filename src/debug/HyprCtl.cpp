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

void HyprCtl::startHyprCtlSocket() {
    int port = 9187;

    std::thread([&]() {
        const auto SOCKET = socket(AF_INET, SOCK_STREAM, 0);

        if (SOCKET < 0) {
            Debug::log(ERR, "Couldn't start the Hyprland Socket. (1) IPC will not work.");
            return;
        }

        const sockaddr_in SERVERADDRESS = {.sin_family = AF_INET, .sin_port = port, .sin_addr = (in_addr)INADDR_ANY};

        if (bind(SOCKET, (sockaddr*)&SERVERADDRESS, sizeof(SERVERADDRESS)) < 0) {
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
            if (request == "R>monitors") reply = monitorsRequest();
            if (request == "R>workspaces") reply = workspacesRequest();
            if (request == "R>clients") reply = clientsRequest();

            write(ACCEPTEDCONNECTION, reply.c_str(), reply.length());

            close(ACCEPTEDCONNECTION);
        }
    }).detach();
}