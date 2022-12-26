#pragma once

#include "../layout/DwindleLayout.hpp"
#include "../layout/MasterLayout.hpp"

class CLayoutManager {
  public:
    IHyprLayout* getCurrentLayout();

    void         switchToLayout(std::string);

  private:
    enum HYPRLAYOUTS {
        LAYOUT_DWINDLE = 0,
        LAYOUT_MASTER
    };

    HYPRLAYOUTS        m_iCurrentLayoutID = LAYOUT_DWINDLE;

    CHyprDwindleLayout m_cDwindleLayout;
    CHyprMasterLayout  m_cMasterLayout;
};

inline std::unique_ptr<CLayoutManager> g_pLayoutManager;