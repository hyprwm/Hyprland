#pragma once

#include "../layout/DwindleLayout.hpp"
#include "../layout/MasterLayout.hpp"

class CLayoutManager {
  public:
    CLayoutManager();

    IHyprLayout*             getCurrentLayout();

    void                     switchToLayout(std::string);

    bool                     addLayout(const std::string& name, IHyprLayout* layout);
    bool                     removeLayout(IHyprLayout* layout);
    std::vector<std::string> getAllLayoutNames();

  private:
    enum eHyprLayouts : uint8_t {
        LAYOUT_DWINDLE = 0,
        LAYOUT_MASTER
    };

    int                                               m_iCurrentLayoutID = LAYOUT_DWINDLE;

    CHyprDwindleLayout                                m_cDwindleLayout;
    CHyprMasterLayout                                 m_cMasterLayout;
    std::vector<std::pair<std::string, IHyprLayout*>> m_vLayouts;
};

inline std::unique_ptr<CLayoutManager> g_pLayoutManager;
