#pragma once

#include "../layout/DwindleLayout.hpp"
#include "../layout/MasterLayout.hpp"

class CLayoutManager {
  public:
    CLayoutManager();

    CIHyprLayout*            getCurrentLayout();

    void                     switchToLayout(std::string);

    bool                     addLayout(const std::string& name, CIHyprLayout* layout);
    bool                     removeLayout(CIHyprLayout* layout);
    std::vector<std::string> getAllLayoutNames();

  private:
    enum eHyprLayouts : uint8_t {
        LAYOUT_DWINDLE = 0,
        LAYOUT_MASTER
    };

    int                                                m_iCurrentLayoutID = LAYOUT_DWINDLE;

    CHyprDwindleLayout                                 m_cDwindleLayout;
    CHyprMasterLayout                                  m_cMasterLayout;
    std::vector<std::pair<std::string, CIHyprLayout*>> m_vLayouts;
};

inline UP<CLayoutManager> g_pLayoutManager;
