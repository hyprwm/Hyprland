#pragma once

#include "../layout/DwindleLayout.hpp"
#include "../layout/MasterLayout.hpp"

class CLayoutManager {
  public:
    CLayoutManager();

    IHyprLayout*             getCurrentLayout(std::optional<int> = std::nullopt);

    void                     switchToLayout(std::string);

    bool                     addLayout(const std::string& name, IHyprLayout* layout);
    bool                     removeLayout(IHyprLayout* layout);
    std::vector<std::string> getAllLayoutNames();

  private:
    enum eHyprLayouts : uint8_t {
        LAYOUT_DWINDLE = 0,
        LAYOUT_MASTER
    };

    int                m_iCurrentLayoutID = LAYOUT_DWINDLE;

    CHyprDwindleLayout m_cDwindleLayout;
    CHyprMasterLayout  m_cMasterLayout;
    // NOTE (a.krylov) why not just use unordered_map?
    std::vector<std::pair<std::string, IHyprLayout*>> m_vLayouts;
    std::unordered_map<std::string, int>              m_LayoutNameToID;
};

inline std::unique_ptr<CLayoutManager> g_pLayoutManager;
