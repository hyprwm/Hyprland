#include "HyprCtl.hpp"

#include <sys/stat.h>
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

void HyprCtl::tickHyprCtl() {
    struct stat buf;

    if (stat("/tmp/hypr/.hyprlandrq", &buf) == 0) {
        // file exists, let's open it

        requestStream.open("/tmp/hypr/.hyprlandrq");

        std::string request = "";
        std::getline(requestStream, request);

        requestStream.close();
        
        std::string reply = "";
        if (request == "R>monitors") reply = monitorsRequest();
        if (request == "R>workspaces") reply = workspacesRequest();
        if (request == "R>clients") reply = clientsRequest();

        if (reply != "") {
            std::ofstream ofs;
            ofs.open("/tmp/hypr/.hyprlandrq", std::ios::trunc);
            ofs << "RPLY:\n" << reply;
            ofs.close();
        }

        // the hyprctl app deletes the file when done.
    }
}