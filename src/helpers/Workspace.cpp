#include "Workspace.hpp"
#include "../Compositor.hpp"

CWorkspace::CWorkspace(int monitorID, std::string name, bool special) {
    const auto PMONITOR = g_pCompositor->getMonitorFromID(monitorID);

    if (!PMONITOR) {
        Debug::log(ERR, "Attempted a creation of CWorkspace with an invalid monitor?");
        return;
    }

    m_iMonitorID          = monitorID;
    m_szName              = name;
    m_bIsSpecialWorkspace = special;

    m_vRenderOffset.m_pWorkspace = this;
    m_vRenderOffset.create(AVARTYPE_VECTOR, special ? g_pConfigManager->getAnimationPropertyConfig("specialWorkspace") : g_pConfigManager->getAnimationPropertyConfig("workspaces"),
                           nullptr, AVARDAMAGE_ENTIRE);
    m_fAlpha.m_pWorkspace = this;
    m_fAlpha.create(AVARTYPE_FLOAT, special ? g_pConfigManager->getAnimationPropertyConfig("specialWorkspace") : g_pConfigManager->getAnimationPropertyConfig("workspaces"),
                    nullptr, AVARDAMAGE_ENTIRE);
    m_fAlpha.setValueAndWarp(1.f);

    m_vRenderOffset.registerVar();
    m_fAlpha.registerVar();

    g_pEventManager->postEvent({"createworkspace", m_szName});
    EMIT_HOOK_EVENT("createWorkspace", this);
}

CWorkspace::~CWorkspace() {
    m_vRenderOffset.unregister();

    Debug::log(LOG, "Destroying workspace ID {}", m_iID);

    g_pEventManager->postEvent({"destroyworkspace", m_szName});
    EMIT_HOOK_EVENT("destroyWorkspace", this);
}

void CWorkspace::startAnim(bool in, bool left, bool instant) {
    const auto         ANIMSTYLE     = m_fAlpha.m_pConfig->pValues->internalStyle;
    static auto* const PWORKSPACEGAP = (Hyprlang::INT* const*)g_pConfigManager->getConfigValuePtr("general:gaps_workspaces");

    if (ANIMSTYLE.starts_with("slidefade")) {
        const auto PMONITOR = g_pCompositor->getMonitorFromID(m_iMonitorID);
        float      movePerc = 100.f;

        if (ANIMSTYLE.find("%") != std::string::npos) {
            try {
                auto percstr = ANIMSTYLE.substr(ANIMSTYLE.find_last_of(' ') + 1);
                movePerc     = std::stoi(percstr.substr(0, percstr.length() - 1));
            } catch (std::exception& e) { Debug::log(ERR, "Error in startAnim: invalid percentage"); }
        }

        m_fAlpha.setValueAndWarp(1.f);
        m_vRenderOffset.setValueAndWarp(Vector2D(0, 0));

        if (ANIMSTYLE.starts_with("slidefadevert")) {
            if (in) {
                m_fAlpha.setValueAndWarp(0.f);
                m_vRenderOffset.setValueAndWarp(Vector2D(0, (left ? PMONITOR->vecSize.y : -PMONITOR->vecSize.y) * (movePerc / 100.f)));
                m_fAlpha        = 1.f;
                m_vRenderOffset = Vector2D(0, 0);
            } else {
                m_fAlpha.setValueAndWarp(1.f);
                m_fAlpha        = 0.f;
                m_vRenderOffset = Vector2D(0, (left ? -PMONITOR->vecSize.y : PMONITOR->vecSize.y) * (movePerc / 100.f));
            }
        } else {
            if (in) {
                m_fAlpha.setValueAndWarp(0.f);
                m_vRenderOffset.setValueAndWarp(Vector2D((left ? PMONITOR->vecSize.x : -PMONITOR->vecSize.x) * (movePerc / 100.f), 0));
                m_fAlpha        = 1.f;
                m_vRenderOffset = Vector2D(0, 0);
            } else {
                m_fAlpha.setValueAndWarp(1.f);
                m_fAlpha        = 0.f;
                m_vRenderOffset = Vector2D((left ? -PMONITOR->vecSize.x : PMONITOR->vecSize.x) * (movePerc / 100.f), 0);
            }
        }
    } else if (ANIMSTYLE == "fade") {
        m_vRenderOffset.setValueAndWarp(Vector2D(0, 0)); // fix a bug, if switching from slide -> fade.

        if (in) {
            m_fAlpha.setValueAndWarp(0.f);
            m_fAlpha = 1.f;
        } else {
            m_fAlpha.setValueAndWarp(1.f);
            m_fAlpha = 0.f;
        }
    } else if (ANIMSTYLE == "slidevert") {
        // fallback is slide
        const auto PMONITOR  = g_pCompositor->getMonitorFromID(m_iMonitorID);
        const auto YDISTANCE = PMONITOR->vecSize.y + **PWORKSPACEGAP;

        m_fAlpha.setValueAndWarp(1.f); // fix a bug, if switching from fade -> slide.

        if (in) {
            m_vRenderOffset.setValueAndWarp(Vector2D(0, left ? YDISTANCE : -YDISTANCE));
            m_vRenderOffset = Vector2D(0, 0);
        } else {
            m_vRenderOffset = Vector2D(0, left ? -YDISTANCE : YDISTANCE);
        }
    } else {
        // fallback is slide
        const auto PMONITOR  = g_pCompositor->getMonitorFromID(m_iMonitorID);
        const auto XDISTANCE = PMONITOR->vecSize.x + **PWORKSPACEGAP;

        m_fAlpha.setValueAndWarp(1.f); // fix a bug, if switching from fade -> slide.

        if (in) {
            m_vRenderOffset.setValueAndWarp(Vector2D(left ? XDISTANCE : -XDISTANCE, 0));
            m_vRenderOffset = Vector2D(0, 0);
        } else {
            m_vRenderOffset = Vector2D(left ? -XDISTANCE : XDISTANCE, 0);
        }
    }

    if (m_bIsSpecialWorkspace) {
        // required for open/close animations
        if (in) {
            m_fAlpha.setValueAndWarp(0.f);
            m_fAlpha = 1.f;
        } else {
            m_fAlpha.setValueAndWarp(1.f);
            m_fAlpha = 0.f;
        }
    }

    if (instant) {
        m_vRenderOffset.warp();
        m_fAlpha.warp();
    }
}

void CWorkspace::setActive(bool on) {
    ; // empty until https://gitlab.freedesktop.org/wayland/wayland-protocols/-/merge_requests/40
}

void CWorkspace::moveToMonitor(const int& id) {
    ; // empty until https://gitlab.freedesktop.org/wayland/wayland-protocols/-/merge_requests/40
}

CWindow* CWorkspace::getLastFocusedWindow() {
    if (!g_pCompositor->windowValidMapped(m_pLastFocusedWindow) || m_pLastFocusedWindow->m_iWorkspaceID != m_iID)
        return nullptr;

    return m_pLastFocusedWindow;
}

void CWorkspace::rememberPrevWorkspace(const CWorkspace* prev) {
    if (!prev) {
        m_sPrevWorkspace.iID  = -1;
        m_sPrevWorkspace.name = "";
        return;
    }

    if (prev->m_iID == m_iID) {
        Debug::log(LOG, "Tried to set prev workspace to the same as current one");
        return;
    }

    m_sPrevWorkspace.iID  = prev->m_iID;
    m_sPrevWorkspace.name = prev->m_szName;
}

std::string CWorkspace::getConfigName() {
    if (m_bIsSpecialWorkspace) {
        return "special:" + m_szName;
    }

    if (m_iID > 0)
        return std::to_string(m_iID);

    return "name:" + m_szName;
}
