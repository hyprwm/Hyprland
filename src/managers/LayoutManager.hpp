#pragma once

#include "../layout/DwindleLayout.hpp"
#include "../layout/MasterLayout.hpp"

class CLayoutManager {
public:

    IHyprLayout*    getCurrentLayout();

    void            switchToLayout(std::string);

private:
    enum HYPRLAYOUTS {
        DWINDLE = 0,
        MASTER
    };

    HYPRLAYOUTS m_iCurrentLayoutID = DWINDLE;

    CHyprDwindleLayout m_cDwindleLayout;
    CHyprMasterLayout  m_cMasterLayout;
};

inline std::unique_ptr<CLayoutManager> g_pLayoutManager;