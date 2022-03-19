#pragma once

#include "../layout/DwindleLayout.hpp"

class CLayoutManager {
public:

    IHyprLayout*    getCurrentLayout();

private:
    enum HYPRLAYOUTS {
        DWINDLE = 0,
    };

    HYPRLAYOUTS m_iCurrentLayoutID = DWINDLE;

    CHyprDwindleLayout m_cDwindleLayout;
};

inline std::unique_ptr<CLayoutManager> g_pLayoutManager;