#include "LayoutManager.hpp"

IHyprLayout* CLayoutManager::getCurrentLayout() {
    switch (m_iCurrentLayoutID) {
        case LAYOUT_DWINDLE: return &m_cDwindleLayout;
        case LAYOUT_MASTER: return &m_cMasterLayout;
    }

    // fallback
    return &m_cDwindleLayout;
}

void CLayoutManager::switchToLayout(std::string layout) {
    if (layout == "dwindle") {
        if (m_iCurrentLayoutID != LAYOUT_DWINDLE) {
            getCurrentLayout()->onDisable();
            m_iCurrentLayoutID = LAYOUT_DWINDLE;
            getCurrentLayout()->onEnable();
        }
    } else if (layout == "master") {
        if (m_iCurrentLayoutID != LAYOUT_MASTER) {
            getCurrentLayout()->onDisable();
            m_iCurrentLayoutID = LAYOUT_MASTER;
            getCurrentLayout()->onEnable();
        }
    } else {
        Debug::log(ERR, "Unknown layout %s!", layout.c_str());
    }
}
