#include "LayoutManager.hpp"

IHyprLayout* CLayoutManager::getCurrentLayout() {
    switch (m_iCurrentLayoutID) {
        case DWINDLE: 
            return &m_cDwindleLayout;
        case MASTER:
            return &m_cMasterLayout;
    }

    // fallback
    return &m_cDwindleLayout;
}

void CLayoutManager::switchToLayout(std::string layout) {
    if (layout == "dwindle") {
        if (m_iCurrentLayoutID != DWINDLE) {
            getCurrentLayout()->onDisable();
            m_iCurrentLayoutID = DWINDLE;
            getCurrentLayout()->onEnable();
        }
    } else if (layout == "master") {
        if (m_iCurrentLayoutID != MASTER) {
            getCurrentLayout()->onDisable();
            m_iCurrentLayoutID = MASTER;
            getCurrentLayout()->onEnable();
        }
    } else {
        Debug::log(ERR, "Unknown layout %s!", layout.c_str());
    }
}