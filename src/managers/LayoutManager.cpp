#include "LayoutManager.hpp"

IHyprLayout* CLayoutManager::getCurrentLayout() {
    switch (m_iCurrentLayoutID) {
        case DWINDLE: 
            return &m_cDwindleLayout;
    }

    // fallback
    return &m_cDwindleLayout;
}